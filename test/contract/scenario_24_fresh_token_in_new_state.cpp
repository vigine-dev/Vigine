// ---------------------------------------------------------------------------
// Scenario 24 -- Fresh IEngineToken minted after an engine-driven FSM
//                transition is bound to the new state and resolves
//                normally; previously-minted tokens stay expired.
//
// Scenario 23 pins the "StateA token expires on transition" half of the
// R-StateScope contract under the live engine path. Scenario 24 pins
// the symmetric half: once the FSM has moved to StateB, a fresh token
// minted via @ref IContext::makeEngineToken(StateB) observes itself
// live, its gated accessors resolve normally, and a token bound to a
// state the FSM never re-enters stays expired regardless of how long
// the engine continues to spin.
//
// The scenario builds on the same end-to-end shape:
//
//   1. Engine started via @ref engine::createEngine.
//   2. Two states registered; flows registered on each via
//      @ref IStateMachine::addStateTaskFlow so the engine pumps
//      whichever flow matches the active state.
//   3. The engine runs on a helper thread; transitions are requested
//      from the test thread via @ref IStateMachine::requestTransition
//      and applied by the engine's controller-thread drain.
//   4. Tokens are minted from the test thread before and after the
//      transition; the test asserts on their alive flags and gated
//      accessor outcomes.
//
// What the scenario verifies (two TEST_F cases below):
//
//   1. FreshTokenMintedAfterTransitionIsBoundToNewState --
//      After the engine drains the StateA->StateB transition, a fresh
//      token minted via @c makeEngineToken(StateB) reports
//      @c boundState == StateB, @c isAlive() == true, and gated
//      accessors resolve their non-Expired typed reasons (ecs() Ok,
//      entityManager / components Unavailable, service() with the
//      sentinel id NotFound). The previously-minted StateA token
//      stays expired across the same engine-driven transition.
//
//   2. StateBTokenSurvivesUntilAnotherTransitionLeavesStateB --
//      Tokens are bound to specific states, not just "the active
//      state". A fresh StateB token survives every subsequent pump
//      tick the engine applies while StateB stays current. The test
//      drives a second transition (StateB->StateA) and verifies the
//      StateB token then flips to expired in turn -- the same
//      invalidation path applies symmetrically regardless of which
//      state the token was originally bound to.
//
// Threading shape mirrors scenario 23: every gated/ungated accessor
// read happens on the test thread (safe by IEngineToken contract);
// transitions go through requestTransition + the engine's drain;
// shutdown joins the helper thread before the destructors run.
// ---------------------------------------------------------------------------

#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/engineconfig.h"
#include "vigine/api/engine/factory.h"
#include "vigine/api/engine/iengine.h"
#include "vigine/api/engine/iengine_token.h"
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

// Probe task: same shape as scenario 23's. Records every run() call so
// the test can wait for the engine to advance the bound flow at least
// once before issuing a transition request.
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

// Aggregate of the live wiring each TEST_F walks through. Carries
// pointers to two probe tasks (one per state) so each case can wait
// for the right flow to fire before moving on to the next phase.
struct EngineFixtureLite
{
    std::unique_ptr<vigine::engine::IEngine> engine;
    vigine::statemachine::StateId            stateA{};
    vigine::statemachine::StateId            stateB{};
    ProbeTask                               *probeA{nullptr};
    ProbeTask                               *probeB{nullptr};
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

// Build the engine, register two states, and wire a probe TaskFlow to
// each. Returns the bundle ready for run(); the FSM is unbound, so
// every mutator above is safe from the test thread. Each probe pointer
// is published into the fixture only after the matching TaskFlow has
// been successfully moved into the FSM -- a registration failure
// leaves the corresponding probeA/probeB null so callers can ASSERT_NE
// on them before any dereference (the TaskFlow that owned the probe
// is moved into the FSM, so a soft EXPECT_* failure used to leave the
// raw pointer dangling).
[[nodiscard]] EngineFixtureLite buildEngineWithFlowsOnBothStates()
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

    if (!fsm.setInitial(fx.stateA).isSuccess())
    {
        ADD_FAILURE() << "setInitial(StateA) must succeed before run()";
        return fx;
    }

    // Wire a probe flow on each state so a transition between them
    // does not turn into a "no flow registered for current state"
    // shape. Each probe's runCount lets the test wait until the
    // engine has actually pumped the matching flow.
    {
        auto flow        = vigine::taskflow::createTaskFlow();
        auto probeOwned  = std::make_unique<ProbeTask>();
        auto *probeRaw   = probeOwned.get();
        const vigine::taskflow::TaskId probeId = flow->addTask();
        if (!probeId.valid())
        {
            ADD_FAILURE() << "ITaskFlow::addTask must yield a valid id for the StateA probe";
            return fx;
        }
        if (!flow->attachTaskRun(probeId, std::move(probeOwned)).isSuccess())
        {
            ADD_FAILURE() << "ITaskFlow::attachTaskRun must bind the StateA probe runnable";
            return fx;
        }
        if (!flow->setRoot(probeId).isSuccess())
        {
            ADD_FAILURE() << "ITaskFlow::setRoot must position the cursor on the StateA probe";
            return fx;
        }
        if (!fsm.addStateTaskFlow(fx.stateA, std::move(flow)).isSuccess())
        {
            ADD_FAILURE() << "addStateTaskFlow(StateA) must succeed";
            return fx;
        }
        fx.probeA = probeRaw;
    }
    {
        auto flow        = vigine::taskflow::createTaskFlow();
        auto probeOwned  = std::make_unique<ProbeTask>();
        auto *probeRaw   = probeOwned.get();
        const vigine::taskflow::TaskId probeId = flow->addTask();
        if (!probeId.valid())
        {
            ADD_FAILURE() << "ITaskFlow::addTask must yield a valid id for the StateB probe";
            return fx;
        }
        if (!flow->attachTaskRun(probeId, std::move(probeOwned)).isSuccess())
        {
            ADD_FAILURE() << "ITaskFlow::attachTaskRun must bind the StateB probe runnable";
            return fx;
        }
        if (!flow->enqueue(probeId).isSuccess())
        {
            ADD_FAILURE() << "ITaskFlow::enqueue must position the cursor on the StateB probe";
            return fx;
        }
        if (!fsm.addStateTaskFlow(fx.stateB, std::move(flow)).isSuccess())
        {
            ADD_FAILURE() << "addStateTaskFlow(StateB) must succeed";
            return fx;
        }
        fx.probeB = probeRaw;
    }

    return fx;
}

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
// After the engine drains a StateA->StateB transition, a fresh token
// minted via makeEngineToken(StateB) is bound to StateB and observes
// itself live. Its gated accessors answer their non-Expired typed
// reasons (the same shape scenario 21 case 1 verifies pre-transition,
// now verified after the engine has applied a real transition).
// The previously-minted StateA token, captured before the transition,
// stays expired in parallel -- proving per-state isolation under the
// engine path.

TEST(EngineFsmFreshToken, FreshTokenMintedAfterTransitionIsBoundToNewState)
{
    auto fx = buildEngineWithFlowsOnBothStates();
    ASSERT_NE(fx.engine, nullptr);
    ASSERT_NE(fx.probeA, nullptr);
    ASSERT_NE(fx.probeB, nullptr);

    auto &context = fx.engine->context();
    auto &fsm     = context.stateMachine();

    // Mint a token bound to StateA before the engine starts spinning,
    // so the test can verify it goes expired across the transition
    // even as the post-transition StateB token comes alive.
    auto stateAToken = context.makeEngineToken(fx.stateA);
    ASSERT_NE(stateAToken, nullptr);
    ASSERT_EQ(stateAToken->boundState(), fx.stateA);
    ASSERT_TRUE(stateAToken->isAlive());

    std::thread driver(
        [&]()
        {
            const auto r = fx.engine->run();
            EXPECT_TRUE(r.isSuccess());
        });
    DriverGuard guard{fx.engine.get(), &driver};

    // Wait for the engine to pump the StateA flow at least once -- this
    // confirms the FSM-drive path is alive before the test asks for a
    // transition.
    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probeA->runCount() > 0u; },
        std::chrono::milliseconds{1000}));

    // Drive the transition through the engine's drain.
    fsm.requestTransition(fx.stateB);

    // Wait for the StateA token to flip to expired -- the deterministic
    // signal that the transition has been applied.
    ASSERT_TRUE(waitUntil(
        [&]() { return !stateAToken->isAlive(); },
        std::chrono::milliseconds{1000}));

    // Wait for the engine to start pumping StateB so callers observe
    // the FSM has truly moved past StateA: the engine's per-tick
    // lookup uses fsm.current(), so a non-zero probeB run count means
    // current() == StateB at least once on a tick.
    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probeB->runCount() > 0u; },
        std::chrono::milliseconds{1000}));

    // Mint a fresh token bound to StateB -- "fresh token in new state"
    // per the scenario 24 contract. The token must observe itself
    // live (StateB is the active state) and its gated accessors must
    // resolve their non-Expired typed reasons.
    auto stateBToken = context.makeEngineToken(fx.stateB);
    ASSERT_NE(stateBToken, nullptr);
    EXPECT_EQ(stateBToken->boundState(), fx.stateB);
    EXPECT_TRUE(stateBToken->isAlive())
        << "a token minted for the currently-active state must observe "
           "itself live";

    EXPECT_EQ(stateBToken->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Ok);
    EXPECT_EQ(stateBToken->entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Unavailable);
    EXPECT_EQ(stateBToken->components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Unavailable);
    EXPECT_EQ(stateBToken->service(vigine::service::ServiceId{}).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::NotFound)
        << "a fresh StateB token must answer NotFound (registry miss), "
           "not Expired (alive-state gate)";

    // The previously-minted StateA token stays expired in parallel --
    // per-state isolation: leaving StateA invalidated only StateA-bound
    // tokens, and a new StateB-bound token does not retroactively
    // resurrect them.
    EXPECT_FALSE(stateAToken->isAlive());
    EXPECT_EQ(stateAToken->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Expired);

    fx.engine->shutdown();
    driver.join();
    guard.engine = nullptr;
    guard.driver = nullptr;
    EXPECT_FALSE(fx.engine->isRunning());
}

// -- Case 2 ------------------------------------------------------------------
//
// Tokens are bound to specific states, not "the active state". A fresh
// StateB token stays alive across every pump tick the engine applies
// while StateB remains current; the second transition (StateB->StateA)
// flips it to expired through the same invalidation path that flipped
// the StateA token in case 1. This proves the invalidation contract is
// symmetric and per-state, not biased toward the FSM's initial state.

TEST(EngineFsmFreshToken, StateBTokenSurvivesUntilAnotherTransitionLeavesStateB)
{
    auto fx = buildEngineWithFlowsOnBothStates();
    ASSERT_NE(fx.engine, nullptr);
    ASSERT_NE(fx.probeA, nullptr);
    ASSERT_NE(fx.probeB, nullptr);

    auto &context = fx.engine->context();
    auto &fsm     = context.stateMachine();

    std::thread driver(
        [&]()
        {
            const auto r = fx.engine->run();
            EXPECT_TRUE(r.isSuccess());
        });
    DriverGuard guard{fx.engine.get(), &driver};

    // Wait for the engine to pump StateA once, then transition to
    // StateB and wait for the StateB flow to start firing.
    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probeA->runCount() > 0u; },
        std::chrono::milliseconds{1000}));
    fsm.requestTransition(fx.stateB);
    ASSERT_TRUE(waitUntil(
        [&]() { return fx.probeB->runCount() > 0u; },
        std::chrono::milliseconds{1000}));

    // Mint a fresh StateB token. It must observe itself live for as
    // long as the FSM stays on StateB. The FSM is sticky -- a probe
    // task that returned a non-routed Result clears its currTask
    // after one run, so the engine's per-tick pump on StateB falls
    // through to the FSM drain alone after that, but the FSM stays
    // on StateB until a transition is requested. We assert liveness
    // by sleeping past a few pump ticks and confirming the token
    // still reports alive and the FSM has not moved.
    auto stateBToken = context.makeEngineToken(fx.stateB);
    ASSERT_NE(stateBToken, nullptr);
    ASSERT_EQ(stateBToken->boundState(), fx.stateB);
    ASSERT_TRUE(stateBToken->isAlive());

    // Sleep past several pump ticks (default tick = 4 ms; sleeping
    // 50 ms gives the engine a comfortable margin to advance multiple
    // iterations of the main loop without the test being timing-tight).
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // The token has survived multiple pump ticks on StateB without
    // flipping -- the alive-flag is sticky to the bound state, not
    // the per-tick pump. The FSM still reports StateB as the current
    // state because no transition was requested in this window.
    EXPECT_EQ(fsm.current(), fx.stateB)
        << "FSM must stay on StateB while no transition is requested";
    EXPECT_TRUE(stateBToken->isAlive())
        << "StateB token must stay alive across pump ticks while StateB "
           "remains the active state";
    EXPECT_EQ(stateBToken->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Ok);

    // Drive the second transition StateB->StateA through the engine
    // drain. The StateB token must flip to expired through the same
    // invalidation path that flipped the StateA token in case 1.
    fsm.requestTransition(fx.stateA);

    ASSERT_TRUE(waitUntil(
        [&]() { return !stateBToken->isAlive(); },
        std::chrono::milliseconds{1000}))
        << "StateB token must flip to expired when the engine drains "
           "the StateB->StateA transition";

    EXPECT_EQ(stateBToken->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Expired);
    EXPECT_EQ(stateBToken->service(vigine::service::ServiceId{}).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::Expired);

    fx.engine->shutdown();
    driver.join();
    guard.engine = nullptr;
    guard.driver = nullptr;
    EXPECT_FALSE(fx.engine->isRunning());
}

} // namespace
} // namespace vigine::contract
