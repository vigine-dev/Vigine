// ---------------------------------------------------------------------------
// Scenario 23 -- IEngineToken token expires on FSM transition while the
//                Engine is driving the per-state TaskFlow end-to-end.
//
// Scenarios 21 and 22 pin the IEngineToken contract against a freestanding
// IContext aggregator: the test thread mints a token, drives FSM
// transitions synchronously, and asserts the gated/ungated split behaves
// as documented in the R-StateScope hybrid policy. Scenario 23 lifts the
// same contract into the live engine path:
//
//   1. The test builds a real Engine via @ref engine::createEngine, so the
//      aggregator under the hood is the modern AbstractContext that
//      backs every production wiring.
//   2. The FSM is configured with two states (StateA, StateB) and a
//      probe TaskFlow is registered on StateA via
//      @ref IStateMachine::addStateTaskFlow (the FSM-drive surface that
//      landed in #334/#339).
//   3. The Engine is driven on a helper thread so the test thread can
//      mint a token bound to StateA via @ref IContext::makeEngineToken
//      and observe the post-transition gating.
//   4. The transition is requested from a producer thread through
//      @ref IStateMachine::requestTransition (any-thread fire-and-forget);
//      the engine's pump thread drains the queue on its next tick and
//      applies the synchronous transition, which fires the invalidation
//      listener that flips the captured token.
//
// The probe task's only job is to confirm the engine is actively pumping
// the StateA-bound TaskFlow before the transition lands -- if the engine
// never advances the flow, the FSM-drive plumbing under test is silently
// broken and the rest of the assertions would pass for the wrong reason.
//
// What the scenario verifies (three TEST_F cases below):
//
//   1. EnginePumpsBoundTaskFlowAndTokenExpiresOnTransition --
//      Engine pumps the StateA-bound TaskFlow at least once; producer
//      posts requestTransition(StateB); the engine drains the queue and
//      the captured StateA token's gated accessors flip to
//      Code::Expired (ecs / entityManager / components / service)
//      while isAlive() reports false.
//
//   2. InfrastructureAccessorsStayValidAcrossEngineDrivenTransition --
//      The captured StateA token's ungated accessors (threadManager,
//      systemBus, signalEmitter, stateMachine) keep returning live
//      references that point at the engine's real singletons after the
//      engine-driven transition. This is the hybrid-gating contract
//      made observable inside the engine path.
//
//   3. SubscribeExpirationFiresOnceFromEngineDrivenTransition --
//      A subscribeExpiration callback registered on the StateA-bound
//      token before the transition fires exactly once when the engine
//      drains the requested transition, even though the firing happens
//      on the engine's controller thread (not the test thread).
//
// Threading note:
//   Once @ref Engine::run() pins the FSM controller to its pump thread,
//   only that thread may call the synchronous @ref IStateMachine::transition
//   path. The test therefore drives transitions exclusively through
//   @ref IStateMachine::requestTransition + the engine's drain. All
//   token minting and accessor reads happen on the test thread, which is
//   safe by the IEngineToken thread-safety contract (every accessor is
//   safe from any thread; the alive-flag is atomic).
// ---------------------------------------------------------------------------

#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/engineconfig.h"
#include "vigine/api/engine/factory.h"
#include "vigine/api/engine/iengine.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/api/taskflow/factory.h"
#include "vigine/api/taskflow/itaskflow.h"
#include "vigine/api/taskflow/taskid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace vigine::contract
{
namespace
{

// Probe task that records every run() invocation. Used to confirm the
// engine actively pumps the StateA-bound TaskFlow before the transition
// lands. The probe returns Result::Success which has no transition
// wired, so the flow's _currTask field clears after the first run and
// subsequent ticks fall through to the FSM drain alone -- exactly the
// shape the engine smoke suite already exercises.
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

// Aggregate of the live wiring each TEST_F walks through. Built by
// buildEngineWithTwoStates() so every case shares the same setup. The
// helper returns the engine, the two state ids, and a non-owning
// pointer to the probe task that the test inspects to confirm the
// engine pump fired before the transition request.
struct EngineFixtureLite
{
    std::unique_ptr<vigine::engine::IEngine> engine;
    vigine::statemachine::StateId            stateA{};
    vigine::statemachine::StateId            stateB{};
    ProbeTask                               *probe{nullptr};
};

// RAII guard that always shuts the engine down and joins the driver
// thread on scope exit. Tests that start a driver std::thread and then
// run ASSERT_* statements would otherwise let a still-joinable
// std::thread destructor call std::terminate() on an early ASSERT_*
// failure return. The guard makes the cleanup path failure-safe: even
// if an ASSERT_* trips between engine->run() launch and the explicit
// driver.join(), the destructor stops the engine and joins the thread.
struct DriverGuard
{
    vigine::engine::IEngine *engine{nullptr};
    std::thread             *driver{nullptr};

    ~DriverGuard()
    {
        if (engine != nullptr)
        {
            engine->shutdown();
        }
        if (driver != nullptr && driver->joinable())
        {
            driver->join();
        }
    }
};

// Build the engine and wire a probe TaskFlow on StateA. Returns a
// fixture with @c probe == nullptr if any setup step fails so callers
// can ASSERT_NE on it before dereferencing -- the previous EXPECT_*
// soft-failure path could let a failing test reach the live engine
// path with a dangling probe pointer (the TaskFlow that owned the
// probe is moved into the FSM, so a registration failure leaves
// fx.probe pointing into a destroyed task).
[[nodiscard]] EngineFixtureLite buildEngineWithTwoStates()
{
    EngineFixtureLite fx;
    fx.engine = vigine::engine::createEngine(vigine::engine::EngineConfig{});
    if (fx.engine == nullptr)
    {
        ADD_FAILURE() << "createEngine must hand back a live engine";
        return fx;
    }

    auto &fsm = fx.engine->context().stateMachine();
    fx.stateA = fsm.addState();
    fx.stateB = fsm.addState();
    if (!fx.stateA.valid() || !fx.stateB.valid())
    {
        ADD_FAILURE() << "addState must yield valid state ids for both StateA and StateB";
        return fx;
    }

    // Drive the FSM to StateA so a token bound to StateA observes
    // itself live and the later transition to StateB is non-noop. The
    // FSM is unbound here (engine has not entered run() yet), so this
    // mutator is safe from the test thread.
    if (!fsm.setInitial(fx.stateA).isSuccess())
    {
        ADD_FAILURE() << "setInitial(StateA) must succeed before run()";
        return fx;
    }

    // Register a probe TaskFlow on StateA so the engine has something
    // to pump while it spins. The probe records every run() invocation
    // so the test can wait for the first pump tick before asking for
    // the transition. The probe pointer is only published into the
    // fixture after the TaskFlow has been successfully moved into the
    // FSM -- if any step before that fails, fx.probe stays null and
    // the caller's ASSERT_NE bails before any dereference.
    auto flow        = vigine::taskflow::createTaskFlow();
    auto probeOwned  = std::make_unique<ProbeTask>();
    auto *probeRaw   = probeOwned.get();
    const vigine::taskflow::TaskId probeId = flow->addTask();
    if (!probeId.valid())
    {
        ADD_FAILURE() << "ITaskFlow::addTask must yield a valid task id for the probe";
        return fx;
    }
    if (!flow->attachTaskRun(probeId, std::move(probeOwned)).isSuccess())
    {
        ADD_FAILURE() << "ITaskFlow::attachTaskRun must bind the probe runnable";
        return fx;
    }
    if (!flow->enqueue(probeId).isSuccess())
    {
        ADD_FAILURE() << "ITaskFlow::enqueue must position the cursor on the probe";
        return fx;
    }

    if (!fsm.addStateTaskFlow(fx.stateA, std::move(flow)).isSuccess())
    {
        ADD_FAILURE() << "addStateTaskFlow(StateA) must succeed";
        return fx;
    }
    if (fsm.taskFlowFor(fx.stateA) == nullptr)
    {
        ADD_FAILURE() << "taskFlowFor(StateA) must report the registered flow";
        return fx;
    }

    fx.probe = probeRaw;
    return fx;
}

// Wait until @p predicate returns true or the deadline elapses. Returns
// the final predicate value so the caller can branch on it. Polls every
// millisecond -- a generous one-second deadline leaves ample headroom
// against CI jitter while the engine's 4 ms pump tick advances.
template <typename Pred>
bool waitUntil(Pred predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return predicate();
}

// -- Case 1 ------------------------------------------------------------------
//
// Engine drives the StateA-bound TaskFlow; while the engine is pumping,
// the test mints a StateA-bound token and observes its gated accessors
// resolve normally (live state). A producer thread then posts
// requestTransition(StateB). The engine's pump drains the queue on its
// next tick, applies the synchronous transition, and fires the
// invalidation listener that flips the token. Every gated accessor on
// the captured token short-circuits to Code::Expired without touching
// the underlying registry -- the same contract scenario_21 pins outside
// the engine, now observed inside the live engine path.

TEST(EngineFsmTokenLifecycle, EnginePumpsBoundTaskFlowAndTokenExpiresOnTransition)
{
    auto fx = buildEngineWithTwoStates();
    ASSERT_NE(fx.engine, nullptr);
    ASSERT_NE(fx.probe, nullptr)
        << "buildEngineWithTwoStates must publish the probe pointer "
           "only after the TaskFlow has been moved into the FSM";

    auto &context = fx.engine->context();
    auto &fsm     = context.stateMachine();

    // Mint a token bound to StateA before the engine starts running.
    // The engine has not yet pinned the FSM to its pump thread, so the
    // token's invalidation-listener registration runs on the test thread
    // and lands on the FSM's listener registry (which takes a mutex
    // and is safe from any thread).
    auto token = context.makeEngineToken(fx.stateA);
    ASSERT_NE(token, nullptr);
    EXPECT_EQ(token->boundState(), fx.stateA);
    EXPECT_TRUE(token->isAlive());

    // Sanity: gated accessors are not Expired before the transition.
    // ecs() resolves Ok because the aggregator wires a live ECS wrapper;
    // entityManager / components are stub-backed so they answer
    // Unavailable; service() with the invalid sentinel id reports
    // NotFound. None should observe Expired before StateA leaves.
    EXPECT_EQ(token->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Ok);
    EXPECT_EQ(token->entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Unavailable);
    EXPECT_EQ(token->components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Unavailable);
    EXPECT_EQ(token->service(vigine::service::ServiceId{}).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::NotFound);

    // Drive the engine on a helper thread. The engine's run() binds the
    // FSM controller to this thread, so transitions from now on must
    // funnel through requestTransition + the controller-thread drain.
    // The DriverGuard makes the cleanup path failure-safe: any ASSERT_*
    // tripping below this point would otherwise leave the joinable
    // driver std::thread to call std::terminate() in its destructor.
    std::thread driver(
        [&]()
        {
            const auto r = fx.engine->run();
            EXPECT_TRUE(r.isSuccess());
        });
    DriverGuard guard{fx.engine.get(), &driver};

    // Wait until the engine has pumped the StateA-bound flow at least
    // once. This proves the FSM-drive plumbing is alive and the token
    // is being observed in the same wiring the production engine uses.
    const bool pumpFired = waitUntil(
        [&]() { return fx.probe->runCount() > 0u; },
        std::chrono::milliseconds{1000});
    EXPECT_TRUE(pumpFired)
        << "engine must pump the StateA-bound TaskFlow before the test "
           "requests the transition";

    // Post the transition from a producer thread (anything other than
    // the engine's pump thread is fine; the test thread itself works).
    // requestTransition is fire-and-forget; the engine's next tick
    // drains the queue and applies transition(StateB) on the controller
    // thread, which fires the invalidation listener for the token.
    fsm.requestTransition(fx.stateB);

    // Wait until the engine has drained the queue and the token has
    // observed its bound state leave. The drain happens on every pump
    // tick, so a generous one-second deadline covers any CI jitter.
    const bool tokenExpired = waitUntil(
        [&]() { return !token->isAlive(); },
        std::chrono::milliseconds{1000});
    EXPECT_TRUE(tokenExpired)
        << "engine drain must fire the invalidation listener and flip "
           "the captured StateA token to expired";

    // Every gated accessor short-circuits to Expired without touching
    // the underlying registry -- the gate fires before any registry
    // lookup, which is the whole point of the R-StateScope split.
    EXPECT_EQ(token->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Expired);
    EXPECT_EQ(token->entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Expired);
    EXPECT_EQ(token->components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Expired);
    EXPECT_EQ(token->service(vigine::service::ServiceId{}).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::Expired)
        << "the alive-state gate must fire before the registry lookup so "
           "callers cannot observe a NotFound on an expired token";

    // Drop the guard explicitly before the post-shutdown isRunning()
    // assertion so the engine has already stopped pumping when we read
    // it. A scope-exit destructor would still cover the early-return
    // path on any failure above.
    fx.engine->shutdown();
    driver.join();
    guard.engine = nullptr;
    guard.driver = nullptr;
    EXPECT_FALSE(fx.engine->isRunning());
}

// -- Case 2 ------------------------------------------------------------------
//
// The hybrid-gating contract is observable inside the engine path: a
// StateA-bound token's ungated infrastructure accessors (threadManager,
// systemBus, signalEmitter, stateMachine) keep returning live
// references after the engine drains the StateA->StateB transition.
// This is what lets a task's deferred work drain in-flight scheduling
// even after the gated tier flipped to Expired (case 1).
//
// The test compares each ungated accessor against the equivalent
// IContext-side singleton where possible. The signalEmitter case only
// asserts liveness across the transition: the token's stub fallback is
// not reachable through IContext, so the contract is "the reference
// remains the same instance across the transition", same shape as
// scenario_21 case 3.

TEST(EngineFsmTokenLifecycle, InfrastructureAccessorsStayValidAcrossEngineDrivenTransition)
{
    auto fx = buildEngineWithTwoStates();
    ASSERT_NE(fx.engine, nullptr);
    ASSERT_NE(fx.probe, nullptr);

    auto &context = fx.engine->context();
    auto &fsm     = context.stateMachine();

    auto token = context.makeEngineToken(fx.stateA);
    ASSERT_NE(token, nullptr);

    // Snapshot the IContext-side singletons before the engine runs so
    // we can compare addresses across the engine-driven transition.
    auto &ctxThreadManager = context.threadManager();
    auto &ctxSystemBus     = context.systemBus();
    auto &ctxStateMachine  = context.stateMachine();

    // Snapshot the signal emitter address before invalidation so the
    // post-expiration accessor can be matched against the same instance
    // (the private NullSignalEmitter stub is not reachable through
    // IContext, so an IContext-side reference comparison is impossible).
    auto *preExpirationEmitter = &token->signalEmitter();

    // Drive the engine; producer posts the transition; wait for
    // invalidation. Same shape as case 1 but condensed because the
    // assertions of interest are post-transition.
    std::thread driver(
        [&]()
        {
            const auto r = fx.engine->run();
            EXPECT_TRUE(r.isSuccess());
        });
    DriverGuard guard{fx.engine.get(), &driver};

    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probe->runCount() > 0u; },
        std::chrono::milliseconds{1000}));

    fsm.requestTransition(fx.stateB);

    ASSERT_TRUE(waitUntil(
        [&]() { return !token->isAlive(); },
        std::chrono::milliseconds{1000}));

    // Ungated accessors keep returning the engine-lifetime singletons.
    // threadManager / systemBus / stateMachine route the same singleton
    // through both surfaces so we can assert identity directly.
    EXPECT_EQ(&token->threadManager(), &ctxThreadManager)
        << "ungated threadManager must still resolve to the engine-lifetime "
           "singleton after the engine-driven transition";
    EXPECT_EQ(&token->systemBus(), &ctxSystemBus)
        << "ungated systemBus must still resolve to the engine-lifetime "
           "singleton after the engine-driven transition";
    EXPECT_EQ(&token->stateMachine(), &ctxStateMachine)
        << "ungated stateMachine must still resolve to the engine-lifetime "
           "singleton after the engine-driven transition";

    // The signal-emitter stub is not exposed through IContext in the
    // current wiring, so we compare the post-expiration address
    // against the snapshot we took before the transition. This proves
    // the accessor stays bound to the same instance across expiration
    // (the noexcept contract holds; no crash, no short-circuit).
    EXPECT_EQ(&token->signalEmitter(), preExpirationEmitter)
        << "ungated signalEmitter must keep returning the same instance "
           "after the engine-driven transition";

    fx.engine->shutdown();
    driver.join();
    guard.engine = nullptr;
    guard.driver = nullptr;
}

// -- Case 3 ------------------------------------------------------------------
//
// subscribeExpiration registered before the engine drains the
// transition fires exactly once when the engine applies it. The
// callback runs on the engine's controller thread (the FSM
// thread-affinity contract pins listener firing to whichever thread
// executed the synchronous transition path); the test verifies
// observable behaviour, not the firing thread, so the captured fire
// count and the post-drain alive flag are the assertion surface.

TEST(EngineFsmTokenLifecycle, SubscribeExpirationFiresOnceFromEngineDrivenTransition)
{
    auto fx = buildEngineWithTwoStates();
    ASSERT_NE(fx.engine, nullptr);
    ASSERT_NE(fx.probe, nullptr);

    auto &context = fx.engine->context();
    auto &fsm     = context.stateMachine();

    auto token = context.makeEngineToken(fx.stateA);
    ASSERT_NE(token, nullptr);

    std::atomic<int> fireCount{0};
    auto             sub =
        token->subscribeExpiration([&]() { fireCount.fetch_add(1, std::memory_order_acq_rel); });
    ASSERT_NE(sub, nullptr);
    EXPECT_TRUE(sub->active());
    EXPECT_EQ(fireCount.load(std::memory_order_acquire), 0)
        << "callback must not fire on registration";

    std::thread driver(
        [&]()
        {
            const auto r = fx.engine->run();
            EXPECT_TRUE(r.isSuccess());
        });
    DriverGuard guard{fx.engine.get(), &driver};

    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probe->runCount() > 0u; },
        std::chrono::milliseconds{1000}));

    fsm.requestTransition(fx.stateB);

    // Wait until the callback has fired (the engine's drain applied
    // the transition and the listener flipped the alive flag + fired
    // every active subscriber exactly once).
    ASSERT_TRUE(waitUntil(
        [&]() { return fireCount.load(std::memory_order_acquire) == 1; },
        std::chrono::milliseconds{1000}))
        << "engine drain must fire the expiration callback exactly once";

    EXPECT_FALSE(token->isAlive())
        << "token must report expired once the engine drain applied the "
           "transition";

    fx.engine->shutdown();
    driver.join();
    guard.engine = nullptr;
    guard.driver = nullptr;

    // After shutdown the engine has stopped pumping; the fire count
    // must remain at one (no second firing latches against the
    // already-flipped token).
    EXPECT_EQ(fireCount.load(std::memory_order_acquire), 1);
}

} // namespace
} // namespace vigine::contract
