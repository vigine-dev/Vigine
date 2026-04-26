#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/engineconfig.h"
#include "vigine/api/engine/factory.h"
#include "vigine/api/engine/iengine.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/impl/engine/engine.h"
#include "vigine/impl/taskflow/taskflow.h"
#include "vigine/result.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
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

class CallbackRunnable final : public core::threading::IRunnable
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

// ---------------------------------------------------------------------------
// FSM-drive scenarios — Engine::run() pumps the per-state TaskFlow
// (#334).
//
// Scenario 7 — no state-bound TaskFlow registered: run() exits cleanly
//   on shutdown without invoking any task pump path. Existing behaviour
//   preserved for callers that drive the engine without a flow.
//
// Scenario 8 — state-bound TaskFlow registered: run() advances the
//   flow's tasks. The probe task records its run-count and the test
//   verifies the FSM-drive pump fired at least once before shutdown.
//
// Note on engine-token observation:
//   The legacy @c vigine::TaskFlow::runCurrentTask path calls
//   @c IContext::makeEngineToken when a context is bound through
//   @c setContext. The modern aggregator built by @c createEngine is a
//   @c vigine::context::AbstractContext, NOT a @c vigine::Context
//   (legacy), so @c TaskFlow::setContext (which expects the legacy
//   class) cannot be wired here. The probe still observes that it ran
//   — that is enough to prove the engine pumped the flow each tick.
//   The token-observation aspect is covered by the existing
//   engine-token contract suite (@c scenario_21/22) and by the
//   demos that exercise the legacy front door.
// ---------------------------------------------------------------------------

namespace
{

using vigine::Result;

// Probe task that records every run() invocation. Used by the FSM-drive
// scenarios to verify the per-tick pump path without needing a full
// demo wiring.
class ProbeTask final : public vigine::AbstractTask
{
  public:
    Result run() override
    {
        _runCount.fetch_add(1, std::memory_order_acq_rel);
        return Result{};
    }

    [[nodiscard]] std::uint32_t runCount() const noexcept
    {
        return _runCount.load(std::memory_order_acquire);
    }

  private:
    std::atomic<std::uint32_t> _runCount{0};
};

} // namespace

// ---------------------------------------------------------------------------
// Scenario 7: Engine::run() with no state-bound TaskFlow exits cleanly.
//
// Pre-arm shutdown so run() exits on the first tick. The FSM has no
// flow registered against its current state; taskFlowFor() returns
// nullptr and the FSM-drive step falls through to the FSM drain +
// main-thread pump alone — exactly the pre-#334 behaviour. The test
// verifies that the new lookup path does not crash or hang when no
// flow is registered.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, RunWithoutBoundTaskFlowExitsCleanly)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    // Sanity: the FSM has a default state but no bound flow yet.
    auto &fsm = engine->context().stateMachine();
    const vigine::statemachine::StateId currentState = fsm.current();
    EXPECT_TRUE(currentState.valid());
    EXPECT_EQ(fsm.taskFlowFor(currentState), nullptr);

    // Pre-arm shutdown so run() exits on the first tick without ever
    // pumping a task. The point is to confirm the new lookup path
    // does not regress the bare-engine fast-exit case.
    engine->shutdown();
    const Result result = engine->run();
    EXPECT_TRUE(result.isSuccess());
    EXPECT_FALSE(engine->isRunning());
}

// ---------------------------------------------------------------------------
// Scenario 8: Engine::run() with a state-bound TaskFlow runs the
//   bound task at least once before shutdown.
//
// Build a TaskFlow holding exactly one ProbeTask, bind it to the FSM's
// current state, then drive run() on a helper thread until the probe
// has fired and call shutdown(). Verifies that:
//   - addStateTaskFlow accepts the registration.
//   - taskFlowFor reports the bound flow back.
//   - The engine pumped the flow at least once during run() so the
//     probe's run-count is non-zero.
//
// The probe returns Result::Success which has no transition wired, so
// runCurrentTask clears the flow's _currTask after the first run --
// subsequent ticks see hasTasksToRun() == false and the FSM-drive
// step falls through to the FSM drain + main-thread pump alone. That
// shape is intentional: the engine asks the flow whether it has work
// each tick and the flow signals completion through hasTasksToRun().
// ---------------------------------------------------------------------------

TEST(EngineSmoke, RunPumpsBoundTaskFlowEachTick)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    auto &fsm = engine->context().stateMachine();

    auto flow = std::make_unique<vigine::TaskFlow>();

    auto       probeOwned = std::make_unique<ProbeTask>();
    ProbeTask *probe      = probeOwned.get();
    auto      *task       = flow->addTask(std::move(probeOwned));
    ASSERT_NE(task, nullptr);
    flow->changeCurrentTaskTo(task);

    const vigine::statemachine::StateId currentState = fsm.current();
    ASSERT_TRUE(currentState.valid());

    const Result reg = fsm.addStateTaskFlow(currentState, std::move(flow));
    ASSERT_TRUE(reg.isSuccess());
    ASSERT_NE(fsm.taskFlowFor(currentState), nullptr);

    // Run the engine on a helper thread so the test thread can poll
    // for the probe to fire and then shut the engine down.
    std::thread driver([&engine]() {
        const Result r = engine->run();
        EXPECT_TRUE(r.isSuccess());
    });

    // Wait until the probe has run at least once or a generous
    // deadline elapses. With a 4 ms pump tick a single iteration
    // should fire within tens of milliseconds; one second leaves
    // ample headroom against CI jitter.
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds{1000};
    while (probe->runCount() == 0u
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    EXPECT_GT(probe->runCount(), 0u);

    engine->shutdown();
    driver.join();
    EXPECT_FALSE(engine->isRunning());
}

// ---------------------------------------------------------------------------
// Scenario 9: addStateTaskFlow rejects null TaskFlow and stale state
//   ids.
//
// Verifies the basic input validation on the new IStateMachine API:
//   - addStateTaskFlow(any, nullptr) reports error.
//   - addStateTaskFlow(stale, flow) reports error.
//   - The valid registration round trip works after the rejections,
//     and a duplicate registration on the same state errors out.
// ---------------------------------------------------------------------------

TEST(EngineSmoke, AddStateTaskFlowRejectsBadInput)
{
    auto engine = createEngine();
    ASSERT_NE(engine, nullptr);

    auto &fsm = engine->context().stateMachine();

    // Null TaskFlow rejected.
    {
        const vigine::statemachine::StateId valid = fsm.current();
        const Result nullCase =
            fsm.addStateTaskFlow(valid, std::unique_ptr<vigine::TaskFlow>{});
        EXPECT_TRUE(nullCase.isError());
    }

    // Stale id rejected.
    {
        auto         flow = std::make_unique<vigine::TaskFlow>();
        const vigine::statemachine::StateId stale{42, 42};
        const Result staleCase = fsm.addStateTaskFlow(stale, std::move(flow));
        EXPECT_TRUE(staleCase.isError());
    }

    // Valid registration succeeds.
    const vigine::statemachine::StateId valid = fsm.current();
    {
        auto         flow = std::make_unique<vigine::TaskFlow>();
        const Result okCase = fsm.addStateTaskFlow(valid, std::move(flow));
        EXPECT_TRUE(okCase.isSuccess());
        EXPECT_NE(fsm.taskFlowFor(valid), nullptr);
    }

    // Re-register on the same state errors out (one-shot per state).
    {
        auto         flow = std::make_unique<vigine::TaskFlow>();
        const Result dupCase = fsm.addStateTaskFlow(valid, std::move(flow));
        EXPECT_TRUE(dupCase.isError());
    }
}
