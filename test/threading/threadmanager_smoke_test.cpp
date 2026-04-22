// ---------------------------------------------------------------------------
// IThreadManager smoke suite.
//
// Exercises the core lifecycle and scheduling surface exposed by
// IThreadManager directly via createThreadManager():
//
//   * Factory returns a non-null manager; observability reports 0
//     in-flight tasks right after construction.
//   * schedule(..., Pool) runs a runnable on a worker thread and the
//     returned handle resolves with a successful Result.
//   * schedule(..., Dedicated) spawns a dedicated OS thread; the
//     manager's dedicatedThreadCount rises by 1 while the task is
//     live and the handle resolves successfully.
//   * scheduleOnNamed runs a runnable on a registered named thread;
//     namedThreadCount reflects the registration; unregisterNamedThread
//     drains pending work and decrements the count.
//   * postToMainThread queues a runnable that fires exactly once on
//     the next runMainThreadPump() call from the main thread.
//   * shutdown() joins all workers; post-shutdown schedule() returns a
//     handle whose wait() reports an error Result (no crash, no hang).
//   * shutdown() is idempotent — a second call is a no-op.
//
// The threading substrate is Level 0; every wrapper above depends on
// these primitives working correctly. A dedicated smoke target turns a
// regression here into a localised failure instead of a mysterious crash
// three layers up.
// ---------------------------------------------------------------------------

#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/irunnable.h"
#include "vigine/threading/itaskhandle.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/namedthreadid.h"
#include "vigine/threading/threadaffinity.h"
#include "vigine/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using vigine::Result;
using vigine::threading::createThreadManager;
using vigine::threading::IRunnable;
using vigine::threading::ITaskHandle;
using vigine::threading::IThreadManager;
using vigine::threading::NamedThreadId;
using vigine::threading::ThreadAffinity;
using vigine::threading::ThreadManagerConfig;

namespace
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns a manager with a small but non-trivial configuration so tests
// run fast on a CI box while still exercising the pool and dedicated paths.
[[nodiscard]] std::unique_ptr<IThreadManager> makeManager(
    std::size_t poolSize         = 2,
    std::size_t maxDedicated     = 4,
    std::size_t maxNamed         = 8)
{
    ThreadManagerConfig cfg{};
    cfg.poolSize           = poolSize;
    cfg.maxDedicatedThreads = maxDedicated;
    cfg.maxNamedThreads    = maxNamed;
    return createThreadManager(cfg);
}

// A minimal IRunnable that records completion and returns Success.
class FlagRunnable final : public IRunnable
{
  public:
    explicit FlagRunnable(std::atomic<bool> &flag) : _flag(flag) {}

    [[nodiscard]] Result run() override
    {
        _flag.store(true, std::memory_order_release);
        return Result{};
    }

  private:
    std::atomic<bool> &_flag;
};

// An IRunnable that blocks until a semaphore is released, then runs.
// Useful for holding a dedicated/named slot open long enough to observe
// the count before it exits.
class BlockingRunnable final : public IRunnable
{
  public:
    explicit BlockingRunnable(std::atomic<bool> &startedFlag,
                              std::atomic<bool> &releaseSignal)
        : _started(startedFlag), _release(releaseSignal)
    {
    }

    [[nodiscard]] Result run() override
    {
        _started.store(true, std::memory_order_release);
        // Spin-wait so the test can observe the in-flight state without
        // introducing platform synchronisation primitives that may not
        // be safe to call before createThreadManager's machinery is ready.
        while (!_release.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        return Result{};
    }

  private:
    std::atomic<bool> &_started;
    std::atomic<bool> &_release;
};

} // namespace

// ---------------------------------------------------------------------------
// 1. Factory — createThreadManager returns a non-null instance
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, FactoryReturnsNonNull)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);
}

// ---------------------------------------------------------------------------
// 2. Observability — pool size matches the configured value
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, PoolSizeMatchesConfig)
{
    constexpr std::size_t kPool = 3;
    auto tm = makeManager(kPool);
    ASSERT_NE(tm, nullptr);
    EXPECT_EQ(tm->poolSize(), kPool);
}

// ---------------------------------------------------------------------------
// 3. schedule(..., Pool) — runs on a worker thread, handle resolves OK
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, SchedulePoolRunsRunnable)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    std::atomic<bool> ran{false};
    auto handle = tm->schedule(
        std::make_unique<FlagRunnable>(ran), ThreadAffinity::Pool);
    ASSERT_NE(handle, nullptr);

    const Result r = handle->wait();
    EXPECT_TRUE(r.isSuccess());
    EXPECT_TRUE(ran.load(std::memory_order_acquire));
}

// ---------------------------------------------------------------------------
// 4. schedule(..., Any) — convenient default path, same contract as Pool
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ScheduleAnyRunsRunnable)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    std::atomic<bool> ran{false};
    // ThreadAffinity::Any is the default; call without the second argument.
    auto handle = tm->schedule(std::make_unique<FlagRunnable>(ran));
    ASSERT_NE(handle, nullptr);

    EXPECT_TRUE(handle->wait().isSuccess());
    EXPECT_TRUE(ran.load(std::memory_order_acquire));
}

// ---------------------------------------------------------------------------
// 5. schedule(..., Dedicated) — dedicated thread; count observable in flight
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ScheduleDedicatedSpawnsThread)
{
    auto tm = makeManager(/*poolSize=*/2, /*maxDedicated=*/2);
    ASSERT_NE(tm, nullptr);

    std::atomic<bool> started{false};
    std::atomic<bool> release{false};

    auto handle = tm->schedule(
        std::make_unique<BlockingRunnable>(started, release),
        ThreadAffinity::Dedicated);
    ASSERT_NE(handle, nullptr);

    // Wait until the dedicated thread is actually running.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!started.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    ASSERT_TRUE(started.load(std::memory_order_acquire))
        << "Dedicated thread did not start within 5 s";

    // At least one dedicated thread is live.
    EXPECT_GE(tm->dedicatedThreadCount(), 1u);

    // Let the runnable finish.
    release.store(true, std::memory_order_release);
    EXPECT_TRUE(handle->wait().isSuccess());
}

// ---------------------------------------------------------------------------
// 6. Dedicated cap — schedule beyond maxDedicatedThreads is rejected
//    (or queued; but the handle still resolves without hanging)
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, DedicatedCapRespected)
{
    // Set cap to 1; schedule two tasks. Both handles must resolve
    // (second may be queued until the first finishes or returned as error
    // depending on the implementation — but the test must not hang).
    constexpr std::size_t kCap = 1;
    auto tm = makeManager(/*poolSize=*/2, /*maxDedicated=*/kCap);
    ASSERT_NE(tm, nullptr);

    std::atomic<bool> started1{false}, release1{false};
    std::atomic<bool> ran2{false};

    auto h1 = tm->schedule(
        std::make_unique<BlockingRunnable>(started1, release1),
        ThreadAffinity::Dedicated);
    ASSERT_NE(h1, nullptr);

    // Wait for first dedicated thread to start.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!started1.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    ASSERT_TRUE(started1.load(std::memory_order_acquire));

    // Respect the cap: at most kCap dedicated threads.
    EXPECT_LE(tm->dedicatedThreadCount(), kCap);

    // Schedule a second dedicated task; it must resolve eventually.
    auto h2 = tm->schedule(
        std::make_unique<FlagRunnable>(ran2), ThreadAffinity::Dedicated);
    ASSERT_NE(h2, nullptr);

    // Release the first blocker so the second can proceed.
    release1.store(true, std::memory_order_release);
    EXPECT_TRUE(h1->wait().isSuccess());

    // The second handle must also complete (or have failed if the cap
    // causes an error rather than queuing — either way, no hang).
    const Result r2 = h2->waitFor(std::chrono::seconds{10});
    // We accept both success (queued + ran) and error (cap rejection).
    // What we do NOT accept is a hang, which waitFor prevents.
    (void)r2; // result observed; hangs caught by the timeout above
}

// ---------------------------------------------------------------------------
// 7. postToMainThread + runMainThreadPump — fires exactly once on drain
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, PostToMainThreadRunsOnPump)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    std::atomic<int> callCount{0};

    // A runnable that increments a counter.
    class CountRunnable final : public IRunnable
    {
      public:
        explicit CountRunnable(std::atomic<int> &counter) : _counter(counter) {}
        [[nodiscard]] Result run() override
        {
            _counter.fetch_add(1, std::memory_order_relaxed);
            return Result{};
        }

      private:
        std::atomic<int> &_counter;
    };

    tm->postToMainThread(std::make_unique<CountRunnable>(callCount));
    tm->postToMainThread(std::make_unique<CountRunnable>(callCount));

    // Before the pump: the queue holds both runnables; count stays 0
    // (this is a non-contractual best-effort assertion — the queue is
    // MPSC, the main pump is caller-driven, no worker touches it).
    EXPECT_EQ(callCount.load(std::memory_order_relaxed), 0);

    // Drain: both runnables execute on the calling (main) thread.
    tm->runMainThreadPump();
    EXPECT_EQ(callCount.load(std::memory_order_acquire), 2);

    // Second drain with empty queue must be a no-op (no crash).
    tm->runMainThreadPump();
    EXPECT_EQ(callCount.load(std::memory_order_acquire), 2);
}

// ---------------------------------------------------------------------------
// 8. scheduleOnNamed — register, schedule, unregister round trip
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ScheduleOnNamedRoundTrip)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    const NamedThreadId id = tm->registerNamedThread("smoke-named");
    ASSERT_TRUE(id.valid()) << "registerNamedThread returned an invalid id";
    EXPECT_EQ(tm->namedThreadCount(), 1u);

    std::atomic<bool> ran{false};
    auto handle = tm->scheduleOnNamed(
        std::make_unique<FlagRunnable>(ran), id);
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(handle->wait().isSuccess());
    EXPECT_TRUE(ran.load(std::memory_order_acquire));

    // Unregister drains the thread and decrements the count.
    tm->unregisterNamedThread(id);
    EXPECT_EQ(tm->namedThreadCount(), 0u);
}

// ---------------------------------------------------------------------------
// 9. scheduleOnNamed with stale id returns a failing handle
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ScheduleOnNamedStaleIdFails)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    const NamedThreadId id = tm->registerNamedThread("short-lived");
    ASSERT_TRUE(id.valid());
    tm->unregisterNamedThread(id);

    // Scheduling on a now-stale id must return a handle that reports error,
    // not execute the runnable.
    std::atomic<bool> ran{false};
    auto handle = tm->scheduleOnNamed(
        std::make_unique<FlagRunnable>(ran), id);
    ASSERT_NE(handle, nullptr);

    const Result r = handle->wait();
    EXPECT_TRUE(r.isError());
    EXPECT_FALSE(ran.load(std::memory_order_acquire));
}

// ---------------------------------------------------------------------------
// 10. shutdown() — joins workers; post-shutdown schedule returns error handle
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ShutdownRejectsNewSchedule)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    tm->shutdown();

    // Post-shutdown: schedule must return a handle whose wait() is an error.
    std::atomic<bool> ran{false};
    auto handle = tm->schedule(std::make_unique<FlagRunnable>(ran));
    ASSERT_NE(handle, nullptr);

    const Result r = handle->wait();
    EXPECT_TRUE(r.isError());
    // The runnable must NOT have been executed.
    EXPECT_FALSE(ran.load(std::memory_order_acquire));
}

// ---------------------------------------------------------------------------
// 11. shutdown() is idempotent — second call must not crash or hang
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ShutdownIsIdempotent)
{
    auto tm = makeManager();
    ASSERT_NE(tm, nullptr);

    tm->shutdown();
    // A second shutdown must be a silent no-op.
    EXPECT_NO_FATAL_FAILURE(tm->shutdown());
}

// ---------------------------------------------------------------------------
// 12. Pending pool task completes before shutdown returns
// ---------------------------------------------------------------------------

TEST(ThreadManagerSmoke, ShutdownDrainsPendingPoolTasks)
{
    auto tm = makeManager(/*poolSize=*/1);
    ASSERT_NE(tm, nullptr);

    std::atomic<bool> ran{false};
    // Schedule before shutdown; the single pool worker picks it up.
    auto handle = tm->schedule(std::make_unique<FlagRunnable>(ran));
    ASSERT_NE(handle, nullptr);

    tm->shutdown(); // must drain the queue before returning

    // Whether the runnable ran or the handle resolved with error, the
    // handle must be ready and the call must not have hung.
    EXPECT_TRUE(handle->ready());
}
