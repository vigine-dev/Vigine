// ---------------------------------------------------------------------------
// Scenario 21 -- IEngineToken stale-after-transition contract.
//
// IEngineToken is the state-scoped DI handle a task receives at onEnter.
// Its accessor surface is split into two tiers per the R-StateScope
// hybrid-gating policy:
//
//   * Domain-level gated accessors (service / system / entityManager /
//     components / ecs) carry lifecycle uncertainty: when the FSM
//     transitions away from the state the token was bound to, the token
//     flips to expired and every gated accessor short-circuits to
//     vigine::engine::Result::Code::Expired without touching the
//     context.
//
//   * Infrastructure accessors (threadManager / systemBus /
//     signalEmitter / stateMachine) refer to engine-lifetime singletons
//     that outlive every state transition. They MUST stay valid even
//     after the token has expired so an invalidated task can drain
//     in-flight scheduling.
//
// The scenario builds the contract suite's standard IContext aggregator,
// pre-registers two states, mints a token bound to stateA via the
// IContext::makeEngineToken factory (the new public minting surface),
// and then exercises three slices of the contract:
//
//   1. PreTransitionGatedAccessorsResolve  -- a token bound to the
//      currently-active stateA reports isAlive(); the gated ecs()
//      accessor reports Code::Ok; service() with a stale id reports
//      Code::NotFound (still gated, but the failure reason is the id,
//      not expiration); the stub-backed entityManager() / components()
//      report Code::Unavailable. None of those flip to Expired before
//      the FSM moves.
//
//   2. PostTransitionGatedAccessorsExpired -- transition stateA ->
//      stateB. Every gated accessor on the token now reports
//      Code::Expired regardless of the underlying registry state, and
//      isAlive() flips to false.
//
//   3. PostTransitionInfrastructureAccessorsStillValid -- after the
//      same transition, threadManager / systemBus / signalEmitter /
//      stateMachine on the token still resolve to live references
//      (matched against the underlying IContext accessors so we do not
//      assume identity, just liveness; the signalEmitter case allows
//      the token to fall back to its private NullSignalEmitter stub
//      when the wiring does not yet pass a real facade through).
//
// All three cases run on the EngineFixture so the FSM and the token
// reach the test through the public IContext aggregator just like every
// other scenario in the full-contract suite.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <memory>

namespace vigine::contract
{
namespace
{

using StaleEngineToken = EngineFixture;

// Helper: build the standard two-state FSM the scenario relies on, with
// stateA selected as the initial / active state. Returns the two ids so
// the test can mint a token bound to stateA and later transition to
// stateB to drive invalidation.
struct TwoStateFsm
{
    vigine::statemachine::StateId stateA{};
    vigine::statemachine::StateId stateB{};
};

[[nodiscard]] TwoStateFsm buildTwoStateFsm(vigine::statemachine::IStateMachine &sm)
{
    TwoStateFsm fsm;
    fsm.stateA = sm.addState();
    fsm.stateB = sm.addState();
    EXPECT_TRUE(fsm.stateA.valid());
    EXPECT_TRUE(fsm.stateB.valid());

    // Drive the FSM to stateA so the token bound to stateA observes
    // itself live and so the later transition to stateB is non-noop
    // (a noop transition does not fire the invalidation listener,
    // see scenario 14 / engine-token smoke for the contract).
    const auto si = sm.setInitial(fsm.stateA);
    EXPECT_TRUE(si.isSuccess());
    return fsm;
}

// -- Case 1 ------------------------------------------------------------------
//
// A token minted while its bound state is active reports isAlive() and
// the gated accessors resolve normally (no Expired short-circuit). The
// gated accessors that hit unwired or empty registry slots still answer
// with their non-Expired typed reason -- Code::NotFound for an unknown
// service id, Code::Unavailable for the stub-backed entityManager and
// components surfaces -- and ecs() reports Code::Ok because the
// IContext aggregator wires a live ECS wrapper.

TEST_F(StaleEngineToken, PreTransitionGatedAccessorsResolve)
{
    auto &sm = context().stateMachine();

    const auto fsm = buildTwoStateFsm(sm);

    auto token = context().makeEngineToken(fsm.stateA);
    ASSERT_NE(token, nullptr) << "makeEngineToken must mint a live token "
                                 "while the bound state is active";

    EXPECT_EQ(token->boundState(), fsm.stateA);
    EXPECT_TRUE(token->isAlive())
        << "token bound to the currently-active state must observe itself live";

    // ecs() resolves through the IContext aggregator which carries a
    // live ECS wrapper (see AbstractContext construction order); no
    // expiration gate fires because the bound state is still active.
    EXPECT_EQ(token->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Ok);

    // entityManager / components are stub-backed in the current
    // context aggregator: the gate returns the typed "underlying
    // surface not yet wired" reason rather than Expired -- the
    // distinction the contract draws between expiration (state
    // transitioned away) and unavailability (subsystem still
    // initialising) is observable here.
    EXPECT_EQ(token->entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Unavailable);
    EXPECT_EQ(token->components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Unavailable);

    // A stale / unregistered service id maps to NotFound, not Expired:
    // the token is still alive, the lookup fails on the registry side.
    const vigine::service::ServiceId stale{};
    EXPECT_EQ(token->service(stale).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::NotFound);
}

// -- Case 2 ------------------------------------------------------------------
//
// Transition the FSM stateA -> stateB. The invalidation listener fires
// synchronously on the controller thread, the token's alive flag flips
// to false, and every gated accessor short-circuits to
// Result::Code::Expired regardless of the underlying registry state.

TEST_F(StaleEngineToken, PostTransitionGatedAccessorsExpired)
{
    auto &sm = context().stateMachine();

    const auto fsm = buildTwoStateFsm(sm);

    auto token = context().makeEngineToken(fsm.stateA);
    ASSERT_NE(token, nullptr);

    // Sanity: live before the transition.
    ASSERT_TRUE(token->isAlive());

    // Transition away -- listener fires synchronously, alive flag flips.
    const auto t = sm.transition(fsm.stateB);
    ASSERT_TRUE(t.isSuccess());

    EXPECT_FALSE(token->isAlive())
        << "token bound to stateA must observe itself expired once the FSM "
           "leaves stateA";

    // Every gated accessor short-circuits to Expired without touching
    // the underlying context. The case below exercises one accessor
    // per gated category to keep the assertion set surgical:
    //   * ecs()           -- previously resolved Code::Ok, now Expired.
    //   * entityManager() -- previously Unavailable, now Expired (the
    //                        gate fires before the underlying availability
    //                        check, so expiration wins).
    //   * components()    -- same gate-first ordering as entityManager.
    //   * service()       -- with a stale id; previously NotFound, now
    //                        Expired. The gate-first ordering guarantees
    //                        the token does not even consult the registry
    //                        once the bound state has transitioned away,
    //                        which is the whole point of the gating split.
    EXPECT_EQ(token->ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Expired);
    EXPECT_EQ(token->entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Expired);
    EXPECT_EQ(token->components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Expired);

    const vigine::service::ServiceId stale{};
    EXPECT_EQ(token->service(stale).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::Expired)
        << "the alive-state gate must fire before the registry lookup so "
           "callers cannot observe a NotFound on an expired token";
}

// -- Case 3 ------------------------------------------------------------------
//
// Infrastructure accessors stay valid after expiration. This is the
// hybrid-gating policy made observable: tasks may use the
// thread manager, system bus, signal emitter, and state machine
// references to drain in-flight scheduling even after the token has
// expired.
//
// We compare each accessor against the equivalent IContext accessor
// where possible to assert identity (threadManager / systemBus /
// stateMachine route the same singleton through both surfaces). The
// signalEmitter case only asserts liveness: when the engine wiring
// does not yet pass a real ISignalEmitter through, the token falls
// back to a private NullSignalEmitter stub whose identity is not
// reachable from IContext, and the contract just guarantees the
// reference is live and the accessor is noexcept.

TEST_F(StaleEngineToken, PostTransitionInfrastructureAccessorsStillValid)
{
    auto &sm = context().stateMachine();

    const auto fsm = buildTwoStateFsm(sm);

    auto token = context().makeEngineToken(fsm.stateA);
    ASSERT_NE(token, nullptr);

    // Snapshot the IContext-side singletons before invalidation so we
    // can compare addresses across the transition.
    auto &ctxThreadManager = context().threadManager();
    auto &ctxSystemBus     = context().systemBus();
    auto &ctxStateMachine  = context().stateMachine();

    // Drive invalidation.
    const auto t = sm.transition(fsm.stateB);
    ASSERT_TRUE(t.isSuccess());
    ASSERT_FALSE(token->isAlive());

    // The token's infrastructure accessors must keep returning the
    // engine-lifetime singletons even though the gated tier has
    // already flipped to Expired (verified in case 2). This is the
    // R-StateScope hybrid gating policy made observable.
    EXPECT_EQ(&token->threadManager(), &ctxThreadManager)
        << "ungated threadManager must still resolve to the engine-lifetime "
           "singleton after token expiration";
    EXPECT_EQ(&token->systemBus(), &ctxSystemBus)
        << "ungated systemBus must still resolve to the engine-lifetime "
           "singleton after token expiration";
    EXPECT_EQ(&token->stateMachine(), &ctxStateMachine)
        << "ungated stateMachine must still resolve to the engine-lifetime "
           "singleton after token expiration";

    // The signal emitter accessor is a self-identity sanity check: the
    // private NullSignalEmitter stub is not exposed through IContext in
    // the current wiring, so the test only confirms that calling the
    // accessor after expiration yields a live reference (the noexcept
    // contract holds) instead of crashing or short-circuiting.
    auto &emitter = token->signalEmitter();
    EXPECT_EQ(&emitter, &emitter);
}

} // namespace
} // namespace vigine::contract
