#include "vigine/threading/defaultthreadmanager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vigine/result.h"
#include "vigine/threading/abstractthreadmanager.h"
#include "vigine/threading/irunnable.h"
#include "vigine/threading/itaskhandle.h"
#include "vigine/threading/namedthreadid.h"
#include "vigine/threading/threadaffinity.h"
#include "vigine/threading/threadmanagerconfig.h"

namespace vigine::threading
{
namespace
{
// -------------------------------------------------------------------------
// Task handle — private concrete carrier of the final Result.
// -------------------------------------------------------------------------
class TaskHandle final : public ITaskHandle
{
  public:
    TaskHandle() = default;
    ~TaskHandle() override = default;

    bool ready() const noexcept override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _done;
    }

    Result wait() override
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return _done; });
        return _result;
    }

    Result waitFor(std::chrono::milliseconds timeout) override
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_cv.wait_for(lock, timeout, [this] { return _done; }))
        {
            return Result{Result::Code::Error, "threading: wait timeout"};
        }
        return _result;
    }

    void cancel() noexcept override
    {
        _cancelRequested.store(true, std::memory_order_release);
    }

    bool cancellationRequested() const noexcept override
    {
        return _cancelRequested.load(std::memory_order_acquire);
    }

    // Called by the worker that runs the runnable.
    void settle(Result result)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_done)
            {
                return;
            }
            _result = std::move(result);
            _done   = true;
        }
        _cv.notify_all();
    }

  private:
    mutable std::mutex              _mutex;
    std::condition_variable         _cv;
    Result                          _result;
    bool                            _done{false};
    std::atomic<bool>               _cancelRequested{false};
};

// -------------------------------------------------------------------------
// Proxy that lets the caller own a unique_ptr<ITaskHandle> while the
// worker keeps a shared_ptr<TaskHandle> alive through the task's lifetime.
// The proxy simply forwards to the shared state.
// -------------------------------------------------------------------------
class TaskHandleProxy final : public ITaskHandle
{
  public:
    explicit TaskHandleProxy(std::shared_ptr<TaskHandle> state) noexcept
        : _state{std::move(state)}
    {
    }

    bool   ready() const noexcept override { return _state->ready(); }
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
    std::shared_ptr<TaskHandle> _state;
};

// -------------------------------------------------------------------------
// Queue entry carrying an owning runnable plus a handle to settle.
// -------------------------------------------------------------------------
struct QueueEntry
{
    std::unique_ptr<IRunnable>  runnable;
    std::shared_ptr<TaskHandle> handle;
};

// -------------------------------------------------------------------------
// Worker-side runner: executes a queue entry and settles the handle.
// Exceptions from IRunnable::run are translated into a failing Result so
// they never escape into the manager's thread body (which would call
// std::terminate).
// -------------------------------------------------------------------------
void runEntry(QueueEntry &entry)
{
    if (!entry.runnable)
    {
        if (entry.handle)
        {
            entry.handle->settle(Result{Result::Code::Error, "threading: null runnable"});
        }
        return;
    }
    if (entry.handle && entry.handle->cancellationRequested())
    {
        entry.handle->settle(Result{Result::Code::Error, "threading: task cancelled"});
        return;
    }
    Result result;
    try
    {
        result = entry.runnable->run();
    }
    catch (const std::exception &e)
    {
        result = Result{Result::Code::Error, e.what() ? e.what() : "threading: runnable threw"};
    }
    catch (...)
    {
        result = Result{Result::Code::Error, "threading: runnable threw non-std exception"};
    }
    if (entry.handle)
    {
        entry.handle->settle(std::move(result));
    }
}

// -------------------------------------------------------------------------
// FIFO queue with a stop flag, condition-variable drain, and wait/pop.
// Used by the worker pool, dedicated slots, and named slots uniformly.
// -------------------------------------------------------------------------
class WorkQueue
{
  public:
    // Posts an entry. Returns false if the queue is already stopped.
    bool push(QueueEntry entry)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_stopped)
            {
                return false;
            }
            _items.push_back(std::move(entry));
        }
        _cv.notify_one();
        return true;
    }

    // Pops an entry, blocking until one is available or the queue stops.
    // Returns std::nullopt when the queue has stopped and is empty.
    std::optional<QueueEntry> pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return _stopped || !_items.empty(); });
        if (_items.empty())
        {
            return std::nullopt;
        }
        QueueEntry entry = std::move(_items.front());
        _items.pop_front();
        return entry;
    }

    // Flip the stop flag and wake every waiter. Remaining items are
    // drained separately via drainCancelled so that handles settle with
    // a failing Result rather than hang.
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
    }

    // Collect every remaining entry and return them to the caller so the
    // caller can settle their handles after stop.
    std::deque<QueueEntry> drainCancelled()
    {
        std::deque<QueueEntry> drained;
        std::lock_guard<std::mutex> lock(_mutex);
        drained.swap(_items);
        return drained;
    }

  private:
    std::mutex              _mutex;
    std::condition_variable _cv;
    std::deque<QueueEntry>  _items;
    bool                    _stopped{false};
};

// Worker thread body used by the pool, dedicated slots, and named slots.
void workerLoop(WorkQueue &queue)
{
    for (;;)
    {
        std::optional<QueueEntry> next = queue.pop();
        if (!next)
        {
            return;
        }
        runEntry(*next);
    }
}

// Drain a stopped queue, settling every handle with a cancellation
// `Result` so waiters do not hang. The reason string travels to every
// settled handle — callers supply it so debug output reflects the
// actual cause (shutdown vs. named-thread unregistration) instead of
// always blaming shutdown.
void drainStoppedQueue(WorkQueue &queue, const char *reason)
{
    auto leftovers = queue.drainCancelled();
    for (auto &entry : leftovers)
    {
        if (entry.handle)
        {
            entry.handle->settle(Result{Result::Code::Error, reason});
        }
    }
}
} // namespace

// =========================================================================
// Impl — the per-manager state, kept opaque to the header.
// =========================================================================
struct DefaultThreadManager::Impl
{
    // Worker pool: N identical workers draining the same queue.
    WorkQueue                pool;
    std::vector<std::thread> poolWorkers;

    // Dedicated slots: one fresh OS thread + one queue per schedule
    // call. The slots live in an append-only vector — there is no
    // caller-identity lookup yet, so the vector index is the only
    // handle. The caller-keyed FIFO-reuse (with a cap from
    // `ThreadManagerConfig::maxDedicatedThreads`) is deferred to a
    // later leaf; until it lands, per-caller FIFO is achieved by
    // reusing a single `NamedThreadId` through
    // `scheduleOnNamed(runnable, id)` instead of `Dedicated`.
    struct DedicatedSlot
    {
        std::unique_ptr<WorkQueue> queue;
        std::thread                worker;
    };
    std::mutex                                      dedicatedMutex;
    std::vector<std::unique_ptr<DedicatedSlot>>     dedicatedSlots;

    // Named-thread slots: owned by index, lifetime tied to
    // registerNamedThread / unregisterNamedThread on the base.
    struct NamedSlot
    {
        std::unique_ptr<WorkQueue> queue;
        std::thread                worker;
    };
    std::mutex                                      namedMutex;
    std::unordered_map<std::size_t,
                       std::unique_ptr<NamedSlot>>  namedSlots;

    // Main-thread queue: drained by runMainThreadPump; no worker.
    std::mutex                   mainMutex;
    std::deque<QueueEntry>       mainQueue;

    // `shutdown()` is documented as safe to call from any thread. The
    // flag that tracks whether shutdown has already run is read and
    // written by those concurrent callers — a plain `bool` would be a
    // data race visible to TSAN. An atomic gives a sequentially
    // consistent ordering with no lock overhead on the hot path.
    std::atomic<bool> shutdownCompleted{false};
};

// =========================================================================
// Construction / destruction.
// =========================================================================
DefaultThreadManager::DefaultThreadManager(ThreadManagerConfig config)
    : AbstractThreadManager(config), _impl{std::make_unique<Impl>()}
{
    const std::size_t workers = AbstractThreadManager::config().poolSize;
    _impl->poolWorkers.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i)
    {
        _impl->poolWorkers.emplace_back([this] { workerLoop(_impl->pool); });
    }
}

DefaultThreadManager::~DefaultThreadManager()
{
    // shutdown() is idempotent and handles the already-shut-down case;
    // calling it from the dtor guarantees deterministic cleanup.
    shutdown();
}

// =========================================================================
// IThreadManager: schedule.
// =========================================================================
std::unique_ptr<ITaskHandle> DefaultThreadManager::schedule(
    std::unique_ptr<IRunnable> runnable, ThreadAffinity affinity)
{
    auto handle = std::make_shared<TaskHandle>();
    auto result = std::unique_ptr<ITaskHandle>(new TaskHandleProxy(handle));

    if (!runnable)
    {
        handle->settle(Result{Result::Code::Error, "threading: null runnable"});
        return result;
    }
    if (isShutDown())
    {
        handle->settle(Result{Result::Code::Error, "threading: manager shut down"});
        return result;
    }

    QueueEntry entry;
    entry.runnable = std::move(runnable);
    entry.handle   = handle;

    switch (affinity)
    {
        case ThreadAffinity::Any:
        case ThreadAffinity::Pool:
        {
            if (!_impl->pool.push(std::move(entry)))
            {
                handle->settle(
                    Result{Result::Code::Error, "threading: pool queue closed"});
            }
            break;
        }

        case ThreadAffinity::Main:
        {
            std::lock_guard<std::mutex> lock(_impl->mainMutex);
            _impl->mainQueue.push_back(std::move(entry));
            break;
        }

        case ThreadAffinity::Dedicated:
        {
            // Fresh dedicated slot per schedule call. Caller-keyed
            // FIFO reuse (with a cap) is on the backlog; until it
            // lands, callers that need predictable per-caller FIFO
            // should reuse a single NamedThreadId through
            // scheduleOnNamed() instead.
            //
            // Publication ordering matters. The worker thread must
            // become visible to `shutdown()` before it is started,
            // otherwise a concurrent shutdown between thread-start
            // and slot-registration would run past an orphan thread
            // that nothing joins. Hence: under `dedicatedMutex`,
            // register the slot FIRST, then start its worker. Because
            // shutdown acquires the same mutex to snapshot the
            // vector, it either runs entirely before we enter the
            // critical section (no slot yet, nothing to join) or
            // entirely after we leave (sees the slot with a
            // joinable worker).
            auto slot   = std::make_unique<Impl::DedicatedSlot>();
            slot->queue = std::make_unique<WorkQueue>();
            WorkQueue *q = slot->queue.get();

            {
                std::lock_guard<std::mutex> lock(_impl->dedicatedMutex);
                _impl->dedicatedSlots.push_back(std::move(slot));
                Impl::DedicatedSlot *slotRef = _impl->dedicatedSlots.back().get();
                slotRef->worker = std::thread([q] { workerLoop(*q); });
            }

            if (!q->push(std::move(entry)))
            {
                handle->settle(
                    Result{Result::Code::Error, "threading: dedicated queue closed"});
            }
            acquireDedicatedSlot();
            break;
        }

        case ThreadAffinity::Named:
        {
            // The generic `schedule(runnable, affinity)` entry point has
            // no way to carry a `NamedThreadId`, so a Named request here
            // is always a caller bug. Previously we quietly diverted to
            // the pool, which made the bug invisible. Surface it as an
            // explicit error so the caller sees the missing id and
            // routes through `scheduleOnNamed(runnable, id)` instead.
            handle->settle(Result{
                Result::Code::Error,
                "threading: Named affinity requires a NamedThreadId; "
                "use scheduleOnNamed(runnable, id)"});
            break;
        }
    }

    return result;
}

std::unique_ptr<ITaskHandle> DefaultThreadManager::scheduleOnNamed(
    std::unique_ptr<IRunnable> runnable, NamedThreadId named)
{
    auto handle = std::make_shared<TaskHandle>();
    auto result = std::unique_ptr<ITaskHandle>(new TaskHandleProxy(handle));

    if (!runnable)
    {
        handle->settle(Result{Result::Code::Error, "threading: null runnable"});
        return result;
    }
    if (isShutDown())
    {
        handle->settle(Result{Result::Code::Error, "threading: manager shut down"});
        return result;
    }

    const std::size_t slotIndex = resolveNamedSlot(named);
    if (slotIndex == std::numeric_limits<std::size_t>::max())
    {
        handle->settle(Result{Result::Code::Error, "threading: invalid named id"});
        return result;
    }

    // Lazy-create the per-id queue/worker on first use.
    WorkQueue *queue = nullptr;
    {
        std::lock_guard<std::mutex> lock(_impl->namedMutex);
        auto                        it = _impl->namedSlots.find(slotIndex);
        if (it == _impl->namedSlots.end())
        {
            auto slot    = std::make_unique<Impl::NamedSlot>();
            slot->queue  = std::make_unique<WorkQueue>();
            queue        = slot->queue.get();
            slot->worker = std::thread([queue] { workerLoop(*queue); });
            _impl->namedSlots.emplace(slotIndex, std::move(slot));
        }
        else
        {
            queue = it->second->queue.get();
        }
    }

    QueueEntry entry;
    entry.runnable = std::move(runnable);
    entry.handle   = handle;
    if (!queue->push(std::move(entry)))
    {
        handle->settle(Result{Result::Code::Error, "threading: named queue closed"});
    }
    return result;
}

// =========================================================================
// IThreadManager: main-thread pump.
// =========================================================================
void DefaultThreadManager::postToMainThread(std::unique_ptr<IRunnable> runnable)
{
    // `postToMainThread` returns `void` — callers cannot observe
    // success, failure, or completion through the function boundary.
    // The older implementation still allocated a `TaskHandle`, filled
    // it into the queue entry, and settled it on error / shutdown
    // paths. Nothing could ever read that handle, so the allocation
    // was dead weight on a hot path (main-thread post runs every
    // tick of the engine pump).
    //
    // Fire-and-forget semantics now: early-return on null runnable
    // or a shut-down manager without allocating a handle; the queue
    // entry carries a null handle so `runEntry` skips the handle-
    // based branches naturally.
    if (!runnable)
    {
        return;
    }
    if (isShutDown())
    {
        return;
    }
    QueueEntry entry;
    entry.runnable = std::move(runnable);
    // entry.handle intentionally left null; see the rationale above.
    std::lock_guard<std::mutex> lock(_impl->mainMutex);
    _impl->mainQueue.push_back(std::move(entry));
}

void DefaultThreadManager::runMainThreadPump()
{
    std::deque<QueueEntry> drained;
    {
        std::lock_guard<std::mutex> lock(_impl->mainMutex);
        drained.swap(_impl->mainQueue);
    }
    for (auto &entry : drained)
    {
        runEntry(entry);
    }
}

// =========================================================================
// Named unregister: forward to the base for registry bookkeeping, then
// tear down the associated queue/worker.
// =========================================================================
void DefaultThreadManager::unregisterNamedThread(NamedThreadId id)
{
    // Capture the slot index before the base forgets the generation.
    const std::size_t slotIndex = resolveNamedSlot(id);
    AbstractThreadManager::unregisterNamedThread(id);

    if (slotIndex == std::numeric_limits<std::size_t>::max())
    {
        return;
    }

    std::unique_ptr<Impl::NamedSlot> slot;
    {
        std::lock_guard<std::mutex> lock(_impl->namedMutex);
        auto                        it = _impl->namedSlots.find(slotIndex);
        if (it == _impl->namedSlots.end())
        {
            return;
        }
        slot = std::move(it->second);
        _impl->namedSlots.erase(it);
    }

    if (slot)
    {
        slot->queue->stop();
        if (slot->worker.joinable())
        {
            slot->worker.join();
        }
        drainStoppedQueue(*slot->queue, "threading: named thread unregistered");
    }
}

// =========================================================================
// Shutdown.
// =========================================================================
void DefaultThreadManager::shutdown()
{
    if (!_impl)
    {
        return;
    }
    // Atomic exchange: the first caller to flip the flag runs the
    // full teardown; any concurrent or subsequent caller sees `true`
    // and returns immediately. This replaces the plain-bool double-
    // check idiom that produced a data race under two concurrent
    // `shutdown()` invocations.
    bool expected = false;
    if (!_impl->shutdownCompleted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    markShutDown();

    // Stop + join the worker pool first. Every pool worker blocks in
    // WorkQueue::pop, so the stop() wake-up is what lets them exit.
    _impl->pool.stop();
    for (auto &worker : _impl->poolWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    _impl->poolWorkers.clear();
    drainStoppedQueue(_impl->pool, "threading: manager shut down");

    // Stop + join every dedicated slot.
    std::vector<std::unique_ptr<Impl::DedicatedSlot>> dedicated;
    {
        std::lock_guard<std::mutex> lock(_impl->dedicatedMutex);
        dedicated.swap(_impl->dedicatedSlots);
    }
    for (auto &slot : dedicated)
    {
        if (!slot)
        {
            continue;
        }
        slot->queue->stop();
        if (slot->worker.joinable())
        {
            slot->worker.join();
        }
        drainStoppedQueue(*slot->queue, "threading: manager shut down");
        releaseDedicatedSlot();
    }

    // Stop + join every named slot.
    std::unordered_map<std::size_t, std::unique_ptr<Impl::NamedSlot>> named;
    {
        std::lock_guard<std::mutex> lock(_impl->namedMutex);
        named.swap(_impl->namedSlots);
    }
    for (auto &pair : named)
    {
        auto &slot = pair.second;
        if (!slot)
        {
            continue;
        }
        slot->queue->stop();
        if (slot->worker.joinable())
        {
            slot->worker.join();
        }
        drainStoppedQueue(*slot->queue, "threading: manager shut down");
    }

    // Drain the main-thread queue so its handles settle instead of hang.
    std::deque<QueueEntry> drainedMain;
    {
        std::lock_guard<std::mutex> lock(_impl->mainMutex);
        drainedMain.swap(_impl->mainQueue);
    }
    for (auto &entry : drainedMain)
    {
        if (entry.handle)
        {
            entry.handle->settle(
                Result{Result::Code::Error, "threading: manager shut down"});
        }
    }

    // No trailing store of `shutdownCompleted = true` here: the
    // compare_exchange_strong at the top of the function already set
    // the flag. Repeating the write would be correct but redundant
    // and misleading — callers racing at the top check the flag via
    // the CAS, not via a plain load after the teardown completes.
}

} // namespace vigine::threading
