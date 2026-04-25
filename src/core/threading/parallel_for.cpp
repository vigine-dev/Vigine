#include "vigine/core/threading/parallel_for.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "vigine/result.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadaffinity.h"

namespace vigine::core::threading
{
namespace
{
// -------------------------------------------------------------------------
// Aggregated handle shared by every chunk dispatched for one parallelFor
// call. Each ChunkRunnable holds a shared_ptr to this object and calls
// notifyChunkDone() on completion; the last chunk to finish flips the
// pending counter to 0, records any aggregated error, and notifies
// waiters. The external ITaskHandle returned to the caller (see
// CompositeTaskHandleProxy below) also holds a shared_ptr, so the
// composite stays alive until the caller releases the handle AND every
// chunk has settled, whichever comes last.
// -------------------------------------------------------------------------
class CompositeTaskHandle final : public ITaskHandle
{
  public:
    explicit CompositeTaskHandle(std::size_t pending) noexcept
        : _pending{pending}
    {
        // A pending count of 0 means the range was empty; mark settled
        // immediately so wait() returns without blocking and ready()
        // reports true to a caller that checks before any chunk is ever
        // dispatched.
        if (pending == 0)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _done = true;
        }
    }

    ~CompositeTaskHandle() override = default;

    bool ready() const noexcept override
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _done;
    }

    Result wait() override
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this] { return _done; });
        return _aggregateResult;
    }

    Result waitFor(std::chrono::milliseconds timeout) override
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (!_cv.wait_for(lock, timeout, [this] { return _done; }))
        {
            return Result{Result::Code::Error, "threading: parallelFor wait timeout"};
        }
        return _aggregateResult;
    }

    void cancel() noexcept override
    {
        _cancelRequested.store(true, std::memory_order_release);
    }

    bool cancellationRequested() const noexcept override
    {
        return _cancelRequested.load(std::memory_order_acquire);
    }

    // Called by every ChunkRunnable exactly once when its range has been
    // processed. The first failing chunk wins the aggregate error slot;
    // subsequent failures are discarded to keep the reported Result
    // deterministic (tests can reason about "the first chunk's error
    // propagates" instead of "an arbitrary chunk's error propagates").
    void notifyChunkDone(Result chunkResult)
    {
        bool settleNow = false;
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (chunkResult.isError() && _aggregateResult.isSuccess())
            {
                _aggregateResult = std::move(chunkResult);
            }

            const std::size_t remaining =
                _pending.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0)
            {
                _done     = true;
                settleNow = true;
            }
        }
        if (settleNow)
        {
            _cv.notify_all();
        }
    }

  private:
    mutable std::mutex      _mtx;
    std::condition_variable _cv;
    std::atomic<std::size_t> _pending;
    Result                   _aggregateResult{}; // default = Code::Success.
    bool                     _done{false};
    std::atomic<bool>        _cancelRequested{false};
};

// -------------------------------------------------------------------------
// Caller-owned façade. The caller gets a unique_ptr<ITaskHandle> that
// forwards every method to the shared composite state. Releasing the
// unique_ptr cannot destroy the composite prematurely because every
// in-flight ChunkRunnable also owns a shared_ptr — mirrors the
// TaskHandle / TaskHandleProxy pattern used by ThreadManager itself.
// -------------------------------------------------------------------------
class CompositeTaskHandleProxy final : public ITaskHandle
{
  public:
    explicit CompositeTaskHandleProxy(std::shared_ptr<CompositeTaskHandle> state) noexcept
        : _state{std::move(state)}
    {
    }

    bool ready() const noexcept override { return _state->ready(); }

    Result wait() override { return _state->wait(); }

    Result waitFor(std::chrono::milliseconds timeout) override
    {
        return _state->waitFor(timeout);
    }

    void cancel() noexcept override { _state->cancel(); }

    bool cancellationRequested() const noexcept override
    {
        return _state->cancellationRequested();
    }

  private:
    std::shared_ptr<CompositeTaskHandle> _state;
};

// -------------------------------------------------------------------------
// Runnable that processes one [_begin, _end) sub-range of the overall
// request. The callable is shared across every chunk so that N chunks do
// not each copy-construct the body; the composite handle is shared so
// the last chunk to finish can settle it regardless of completion
// order.
//
// Invariant: the composite's pending counter is decremented exactly
// once per ChunkRunnable instance — either inside run() on the happy
// path, or inside the destructor if the runnable was discarded without
// being run (e.g. the thread manager was shut down between schedule()
// and worker pickup, which destroys the QueueEntry without invoking
// run). Without the destructor fallback, a pre-dispatch failure would
// leak a pending count and parallelFor()'s aggregate handle would hang
// in wait() forever.
// -------------------------------------------------------------------------
class ChunkRunnable final : public IRunnable
{
  public:
    ChunkRunnable(std::shared_ptr<CompositeTaskHandle>             handle,
                  std::shared_ptr<std::function<void(std::size_t)>> body,
                  std::size_t                                       begin,
                  std::size_t                                       end) noexcept
        : _handle{std::move(handle)},
          _body{std::move(body)},
          _begin{begin},
          _end{end}
    {
    }

    ~ChunkRunnable() override
    {
        // Fallback settle path. Only fires when run() never executed —
        // _notified is the once-only guard. Typical case: manager was
        // shut down between schedule() and worker pickup, so the
        // QueueEntry is destroyed with the runnable still inside it.
        if (!_notified && _handle)
        {
            _handle->notifyChunkDone(
                Result{Result::Code::Error,
                       "threading: parallelFor chunk not executed"});
        }
    }

    Result run() override
    {
        // The runnable always reports its own Result upward to
        // ThreadManager, but it ALSO reports the same Result into the
        // composite handle so the aggregated view matches. We have to
        // catch exceptions locally because the composite's pending
        // counter would leak otherwise — the manager-side catch in
        // `runEntry` fires too late (after this function returns), and
        // a throw escaping our own `notifyChunkDone` call would leave
        // the composite stuck one chunk short of settled forever.
        Result chunkResult;

        // Guard against a null or cleared body shared_ptr. A bare
        // std::function<void(std::size_t)>{} is valid-but-empty; calling
        // it throws std::bad_function_call, which the catch below would
        // turn into an error Result. Early-exit makes the intent
        // explicit and the error message precise.
        if (!_body || !*_body)
        {
            chunkResult = Result{Result::Code::Error,
                                 "threading: parallelFor body is empty"};
        }
        else if (_handle && _handle->cancellationRequested())
        {
            chunkResult = Result{Result::Code::Error,
                                 "threading: parallelFor cancelled"};
        }
        else
        {
            try
            {
                for (std::size_t i = _begin; i < _end; ++i)
                {
                    // Cooperative cancellation: check once per index so
                    // a long chunk can bail out mid-flight without
                    // completing every remaining iteration.
                    if (_handle && _handle->cancellationRequested())
                    {
                        chunkResult = Result{Result::Code::Error,
                                             "threading: parallelFor cancelled"};
                        break;
                    }
                    (*_body)(i);
                }
            }
            catch (const std::exception &e)
            {
                chunkResult = Result{Result::Code::Error,
                                     e.what()
                                         ? e.what()
                                         : "threading: parallelFor body threw"};
            }
            catch (...)
            {
                chunkResult = Result{
                    Result::Code::Error,
                    "threading: parallelFor body threw non-std exception"};
            }
        }

        if (_handle)
        {
            _handle->notifyChunkDone(chunkResult);
            _notified = true;
        }
        return chunkResult;
    }

  private:
    std::shared_ptr<CompositeTaskHandle>             _handle;
    std::shared_ptr<std::function<void(std::size_t)>> _body;
    std::size_t                                       _begin;
    std::size_t                                       _end;
    bool                                              _notified{false};
};

} // namespace

// =========================================================================
// Public entry point.
// =========================================================================
std::unique_ptr<ITaskHandle> parallelFor(IThreadManager                      &tm,
                                         std::size_t                          count,
                                         std::function<void(std::size_t)>     body,
                                         ThreadAffinity                       affinity)
{
    // Fast-path: empty range. Return a handle that is already settled
    // with a successful Result so wait() returns immediately. The
    // handle still satisfies the return type so callers do not need a
    // null check.
    if (count == 0)
    {
        auto composite = std::make_shared<CompositeTaskHandle>(
            /*pending=*/static_cast<std::size_t>(0));
        return std::unique_ptr<ITaskHandle>(
            new CompositeTaskHandleProxy(std::move(composite)));
    }

    // Chunk sizing: aim for roughly one chunk per pool worker so the
    // dispatcher saturates the pool without paying N per-index
    // scheduling costs. When `poolSize()` reports 0 (a degenerate
    // manager with no workers, or a non-Pool affinity routed through a
    // manager whose pool is empty), fall back to a single chunk
    // covering the whole range — the pool will still drain it, just
    // serially, and the caller still gets a valid handle.
    const std::size_t poolSize  = tm.poolSize();
    const std::size_t chunkSize = (poolSize == 0)
                                      ? count
                                      : std::max<std::size_t>(1, count / poolSize);

    // Ceiling division: for count = 10, chunkSize = 3, we need 4 chunks
    // covering [0,3), [3,6), [6,9), [9,10).
    const std::size_t numChunks = (count + chunkSize - 1) / chunkSize;

    auto composite =
        std::make_shared<CompositeTaskHandle>(/*pending=*/numChunks);
    auto sharedBody =
        std::make_shared<std::function<void(std::size_t)>>(std::move(body));

    // Build the external handle BEFORE scheduling any chunk. The
    // per-chunk schedule handle is discarded — the composite is the
    // single source of truth the caller waits on.
    auto result = std::unique_ptr<ITaskHandle>(
        new CompositeTaskHandleProxy(composite));

    for (std::size_t begin = 0; begin < count; begin += chunkSize)
    {
        const std::size_t end = std::min(begin + chunkSize, count);
        auto              runnable =
            std::make_unique<ChunkRunnable>(composite, sharedBody, begin, end);

        // Discard the returned per-chunk handle. The composite counter
        // is decremented from exactly one of two sites:
        //   1. ChunkRunnable::run() on the happy path; or
        //   2. ChunkRunnable::~ChunkRunnable() on any path where the
        //      manager destroys the runnable without invoking run()
        //      (e.g. manager shut down between schedule and worker
        //      pickup).
        // That dual-path invariant makes the pending counter settle
        // exactly numChunks times regardless of how each individual
        // schedule call resolves, so parallelFor()'s aggregate handle
        // never hangs.
        (void)tm.schedule(std::move(runnable), affinity);
    }

    return result;
}

} // namespace vigine::core::threading
