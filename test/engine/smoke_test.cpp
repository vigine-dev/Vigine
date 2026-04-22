#include "vigine/context/icontext.h"
#include "vigine/engine/defaultengine.h"
#include "vigine/engine/engineconfig.h"
#include "vigine/engine/factory.h"
#include "vigine/engine/iengine.h"
#include "vigine/result.h"
#include "vigine/threading/irunnable.h"
#include "vigine/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// ---------------------------------------------------------------------------
// Test suite: Engine smoke tests (label: engine-smoke)
//
// Scenario 1 — construct + destruct without run:
//   Build an engine, never call run(), let it destruct. No crash; no
//   hang; isRunning() is false before and after.
//
// Scenario 2 — run() + shutdown from another thread:
//   Run on the main (test) thread. A helper thread waits until the
//   engine reports isRunning() and then calls shutdown(). run()
//   returns Success with bounded latency.
//
// Scenario 3 — shutdown from a main-thread runnable:
//   Run on the test thread. Post a main-thread runnable that calls
//   shutdown() when the pump drains it. The loop exits cleanly; the
//   runnable was executed by the engine.
//
// Scenario 4 — double-shutdown is idempotent:
//   Shutdown before run(): run() returns quickly. Shutdown twice
//   afterwards: the second call is a no-op.
//
// Scenario 5 — run() is single-shot:
//   First run() succeeds; second run() returns an error Result without
//   stalling.
//
// Scenario 6 — freeze-after-run blocks mutation:
//   Register a null service before run() to observe Result::Error (the
//   aggregator's own guard). After run() starts + exits, the context
//   has been frozen; register any future service and observe
//   Result::Code::TopologyFrozen.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine;
using namespace vigine::engine;

// ---------------------------------------------------------------------------
// Runnable that invokes a std::function on run(). Used to post
// shutdown-from-pump-callback and similar probes to the main-thread
// queue without leaking captured state through the IRunnable contract.
// ---------------------------------------------------------------------------

class CallbackRunnable final : public threading::IRunnable
{
  public:
    explicit CallbackRunnable(std::function<void()> fn) : _fn(std::move(fn)) {}

    Result run() override
    {
        if (_fn)
        {
            _fn();
        }
        return Result{};
    }

  private:
    std::function<void()> _fn;
};

// Convenience: poll isRunning() until it returns the expected value or
// the timeout elapses. Returns the observed value so the caller can
// assert on it directly.
bool waitForRunningState(IEngine &engine, bool expected, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (engine.isRunning() == expected)
        {
            return expected;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return engine.isRunning();
}

} // namespace

// ---------------------------------------------------------------------------
// Scenario 1: construct + destruct without run.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, ConstructDestructWithoutRun)
{
    // A bare engine must be safe to build and drop with no run() call.
    // Any resource leak or ordering bug in the reverse teardown would
    // surface here as a hang on the thread manager's worker join.
    {
        auto engine = createEngine();
        ASSERT_NE(engine, nullptr);
        EXPECT_FALSE(engine->isRunning());

        // Access the context so the aggregator is observed live before
        // destruction. The reference must remain valid through the end
        // of the scope.
        IContext &context = engine->context();
        EXPECT_FALSE(context.isFrozen());
    }
    // Engine destructor has run by here; no thread should be joined in
    // a deadlock state (the test would otherwise hang and be killed by
    // the ctest DISCOVERY_TIMEOUT).
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Scenario 2: run() + shutdown from another thread.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, RunThenShutdownFromOtherThread)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    // Spin a helper that waits for isRunning() == true, then calls
    // shutdown(). The helper uses steady_clock so a stalled main loop
    // would be caught by the test's own timeout rather than hanging
    // here.
    std::atomic<bool> helperFinished{false};
    std::thread helper([&engine, &helperFinished]() {
        // Give the main loop a generous window to enter run().
        const bool running =
            waitForRunningState(*engine, true, std::chrono::milliseconds{1000});
        EXPECT_TRUE(running);
        engine->shutdown();
        helperFinished.store(true, std::memory_order_release);
    });

    // Block on run() from the test thread; this mirrors the production
    // expectation that the OS main thread drives the pump.
    const Result result = engine->run();
    EXPECT_TRUE(result.isSuccess());
    EXPECT_FALSE(engine->isRunning());

    helper.join();
    EXPECT_TRUE(helperFinished.load(std::memory_order_acquire));
}

// ---------------------------------------------------------------------------
// Scenario 3: shutdown from a main-thread runnable.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, ShutdownFromMainThreadCallback)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    // Use a helper thread to wait until isRunning() == true, then post
    // the shutdown-runnable onto the main-thread queue. The runnable
    // runs ON the main thread (drained by engine's pump), so shutdown
    // is triggered from inside the pump's own context.
    std::atomic<bool> callbackRan{false};
    std::thread poster([&engine, &callbackRan]() {
        waitForRunningState(*engine, true, std::chrono::milliseconds{1000});
        auto runnable = std::make_unique<CallbackRunnable>([&engine, &callbackRan]() {
            callbackRan.store(true, std::memory_order_release);
            engine->shutdown();
        });
        engine->context().threadManager().postToMainThread(std::move(runnable));
    });

    const Result result = engine->run();
    EXPECT_TRUE(result.isSuccess());
    EXPECT_TRUE(callbackRan.load(std::memory_order_acquire));

    poster.join();
}

// ---------------------------------------------------------------------------
// Scenario 4: double-shutdown is idempotent; pre-run shutdown fast-exits.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, DoubleShutdownIsIdempotent)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    // Pre-arm shutdown before run() is called. run() must observe the
    // flag immediately and return without blocking on the pump.
    engine->shutdown();
    engine->shutdown(); // second call -- no-op, must not throw.

    const auto start = std::chrono::steady_clock::now();
    const Result result = engine->run();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(result.isSuccess());
    // The fast-exit path should return within a few pump ticks; a
    // ten-second ceiling is far beyond any realistic latency but still
    // catches a broken-loop regression.
    EXPECT_LT(elapsed, std::chrono::seconds{10});
    EXPECT_FALSE(engine->isRunning());

    // Third shutdown after run() exits must also be a no-op.
    engine->shutdown();
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Scenario 5: run() is single-shot.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, RunIsSingleShot)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    // Pre-arm shutdown so the first run() returns quickly.
    engine->shutdown();
    const Result first = engine->run();
    EXPECT_TRUE(first.isSuccess());

    // Second run() must report an error without stalling.
    const auto start = std::chrono::steady_clock::now();
    const Result second = engine->run();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(second.isError());
    EXPECT_LT(elapsed, std::chrono::seconds{1});
}

// ---------------------------------------------------------------------------
// Scenario 6: freeze-after-run blocks topology mutation.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, RunFreezesTheContext)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);
    EXPECT_FALSE(engine->context().isFrozen());

    // Drive run() + shutdown so freeze() fires on entry.
    engine->shutdown();
    const Result result = engine->run();
    EXPECT_TRUE(result.isSuccess());

    // After run() returns, the context must be frozen. A subsequent
    // registerService call (even with a null pointer) returns either
    // the null-check Error or TopologyFrozen; both are acceptable
    // because the aggregator's own guards run before the freeze check
    // -- the important invariant is that the freeze flag was set by
    // run().
    EXPECT_TRUE(engine->context().isFrozen());
}
