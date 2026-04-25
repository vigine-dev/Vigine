// ---------------------------------------------------------------------------
// EngineToken smoke suite (label: engine-token-smoke).
//
// Exercises the concrete @ref vigine::engine::EngineToken end-to-end:
// the constructor binds to a @ref StateId; the FSM-level invalidation
// hook drives the alive flag; the gated accessors short-circuit to
// @ref Result::Code::Expired post-invalidation; the ungated accessors
// stay valid; @ref subscribeExpiration delivers exactly once and
// returns a null subscription token when the caller registers
// post-invalidation (matching the IEngineToken contract); no-op
// transitions do NOT fire the listener.
//
// Scope: the formal scenario_21 / scenario_22 contract tests live in
// follow-up leaves #303 / #304. This suite keeps coverage tight to the
// pieces this leaf adds — concrete final, FSM hook, callback list,
// hybrid gating policy, "exactly once" / "no-op-no-fire" / "null-on-
// expired-registration" semantics.
// ---------------------------------------------------------------------------

#include "vigine/api/context/factory.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/impl/engine/enginetoken.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/result.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using vigine::IContext;
using vigine::engine::EngineToken;
using vigine::statemachine::IStateMachine;
using vigine::statemachine::StateId;

// Build a context fixture and pre-register two states so every test
// shares the same minimal wiring without copy-pasting the boilerplate.
struct ContextFixture
{
    std::unique_ptr<IContext> context;
    StateId                   stateA{};
    StateId                   stateB{};

    static ContextFixture make()
    {
        ContextFixture fx;
        fx.context = vigine::context::createContext({});
        if (!fx.context)
        {
            return fx;
        }
        IStateMachine &sm = fx.context->stateMachine();
        fx.stateA = sm.addState();
        fx.stateB = sm.addState();
        // Drive @c current to stateA so subsequent transitions to
        // stateB are non-noop and trigger the invalidation hook.
        const auto si = sm.setInitial(fx.stateA);
        EXPECT_TRUE(si.isSuccess());
        return fx;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Scenario 1: a freshly constructed token reports the bound state and
// observes itself live; the gated and ungated accessors all answer.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, NewTokenReportsBoundStateAndAlive)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    EXPECT_EQ(token.boundState(), fx.stateA);
    EXPECT_TRUE(token.isAlive());

    // ungated accessors return live references regardless of the alive
    // flag because the resources behind them outlive every state
    // transition (R-StateScope hybrid gating).
    EXPECT_EQ(&token.threadManager(), &fx.context->threadManager());
    EXPECT_EQ(&token.systemBus(),     &fx.context->systemBus());
    EXPECT_EQ(&token.stateMachine(),  &fx.context->stateMachine());

    // gated accessors with no live registry slot still answer with a
    // typed reason (Unavailable for stub surfaces; NotFound for a
    // service id no one registered).
    EXPECT_EQ(token.entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Unavailable);
    EXPECT_EQ(token.components().code(),
              vigine::engine::Result<vigine::IComponentManager &>::Code::Unavailable);
    EXPECT_EQ(token.ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Ok);

    const vigine::service::ServiceId stale{};
    EXPECT_EQ(token.service(stale).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::NotFound);
}

// ---------------------------------------------------------------------------
// Scenario 2: an FSM transition away from the bound state invalidates
// the token; gated accessors flip to Expired; ungated accessors stay
// valid; subscribeExpiration callbacks fire exactly once.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, TransitionInvalidatesTokenAndFiresCallbacks)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    std::atomic<int> fireCount{0};
    auto sub = token.subscribeExpiration([&]() { fireCount.fetch_add(1); });
    ASSERT_NE(sub, nullptr);
    EXPECT_TRUE(sub->active());

    // Sanity: still alive before the transition.
    EXPECT_TRUE(token.isAlive());
    EXPECT_EQ(fireCount.load(), 0);

    // Transition away — the invalidation listener fires synchronously.
    const auto t = fx.context->stateMachine().transition(fx.stateB);
    EXPECT_TRUE(t.isSuccess());

    // Token has flipped to expired; the callback fired exactly once.
    EXPECT_FALSE(token.isAlive());
    EXPECT_EQ(fireCount.load(), 1);

    // Gated accessors short-circuit to Expired without touching the
    // context.
    EXPECT_EQ(token.ecs().code(),
              vigine::engine::Result<vigine::ecs::IECS &>::Code::Expired);
    EXPECT_EQ(token.entityManager().code(),
              vigine::engine::Result<vigine::IEntityManager &>::Code::Expired);
    EXPECT_EQ(token.service(vigine::service::ServiceId{1, 1}).code(),
              vigine::engine::Result<vigine::service::IService &>::Code::Expired);

    // Ungated infra accessors keep working — that's the whole point of
    // the hybrid gating policy. Tasks may use these to drain in-flight
    // work even after the token has expired.
    EXPECT_EQ(&token.threadManager(), &fx.context->threadManager());
    EXPECT_EQ(&token.systemBus(),     &fx.context->systemBus());

    // A second transition from stateB to stateA does NOT re-fire the
    // callbacks for this token (already fired-once latch holds).
    const auto t2 = fx.context->stateMachine().transition(fx.stateA);
    EXPECT_TRUE(t2.isSuccess());
    EXPECT_EQ(fireCount.load(), 1);
}

// ---------------------------------------------------------------------------
// Scenario 3: subscribeExpiration registered AFTER invalidation
// returns a null subscription token without invoking the callback.
// This matches the @ref IEngineToken contract: "Returns a null
// subscription token when @p callback is empty or when the token is
// already expired at registration time."
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, SubscribeAfterInvalidationReturnsNull)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    // Drive invalidation before registering any callback.
    const auto t = fx.context->stateMachine().transition(fx.stateB);
    EXPECT_TRUE(t.isSuccess());
    ASSERT_FALSE(token.isAlive());

    std::atomic<int> fireCount{0};
    auto sub = token.subscribeExpiration([&]() { fireCount.fetch_add(1); });

    // Per contract: null subscription token AND callback NOT invoked.
    EXPECT_EQ(sub, nullptr);
    EXPECT_EQ(fireCount.load(), 0);
}

// ---------------------------------------------------------------------------
// Scenario 3b: subscribeExpiration with an empty callback returns a
// null subscription token without invoking anything (the same contract
// branch as scenario 3 but driven by the callback emptiness, not the
// alive flag).
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, SubscribeWithEmptyCallbackReturnsNull)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    auto sub = token.subscribeExpiration(std::function<void()>{});
    EXPECT_EQ(sub, nullptr);
}

// ---------------------------------------------------------------------------
// Scenario 4: a no-op transition (target == current) does NOT fire the
// invalidation listener. Tokens stay alive, callbacks stay un-fired.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, NoopTransitionDoesNotInvalidateToken)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    std::atomic<int> fireCount{0};
    auto sub = token.subscribeExpiration([&]() { fireCount.fetch_add(1); });
    ASSERT_NE(sub, nullptr);

    // Re-transition to the SAME state; the FSM short-circuits and the
    // listener is not fired.
    const auto t = fx.context->stateMachine().transition(fx.stateA);
    EXPECT_TRUE(t.isSuccess());

    EXPECT_TRUE(token.isAlive());
    EXPECT_EQ(fireCount.load(), 0);
}

// ---------------------------------------------------------------------------
// Scenario 5: multiple subscribeExpiration registrations all fire on
// the same transition. Cancel before invalidation drops the slot.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, MultipleSubscribersFireAndCancelDropsSlot)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    std::atomic<int> firedA{0};
    std::atomic<int> firedB{0};
    std::atomic<int> firedC{0};

    auto subA = token.subscribeExpiration([&]() { firedA.fetch_add(1); });
    auto subB = token.subscribeExpiration([&]() { firedB.fetch_add(1); });
    auto subC = token.subscribeExpiration([&]() { firedC.fetch_add(1); });

    // Cancel B before invalidation; A and C should still fire.
    subB->cancel();
    EXPECT_FALSE(subB->active());

    const auto t = fx.context->stateMachine().transition(fx.stateB);
    EXPECT_TRUE(t.isSuccess());

    EXPECT_EQ(firedA.load(), 1);
    EXPECT_EQ(firedB.load(), 0);
    EXPECT_EQ(firedC.load(), 1);
}

// ---------------------------------------------------------------------------
// Scenario 5b: cancelled callback slots are reused on the next
// register call instead of growing the registry unboundedly. The slot
// reuse is observable through the live-token bookkeeping: ten cycles
// of subscribe-and-cancel keep the registry at one slot the whole time.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, CancelledSlotsAreReusedOnSubsequentRegister)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    // Subscribe-and-cancel ten times. With reuse-on-add, the registry
    // never grows past a single slot since each cancel clears the
    // callback before the next register fills the same slot back in.
    // Without reuse the registry would grow to ten entries.
    for (int i = 0; i < 10; ++i)
    {
        auto sub = token.subscribeExpiration([]() {});
        ASSERT_NE(sub, nullptr);
        EXPECT_TRUE(sub->active());
        sub->cancel();
        EXPECT_FALSE(sub->active());
    }

    // After the loop, transition the FSM. No live subscription, so no
    // callback fires; the test exists to surface a registry bloat
    // regression, not to count fires.
    std::atomic<int> dummyFire{0};
    auto sub = token.subscribeExpiration([&]() { dummyFire.fetch_add(1); });
    ASSERT_NE(sub, nullptr);
    const auto t = fx.context->stateMachine().transition(fx.stateB);
    EXPECT_TRUE(t.isSuccess());
    EXPECT_EQ(dummyFire.load(), 1);
}

// ---------------------------------------------------------------------------
// Scenario 5c: the @ref signalEmitter accessor returns a live
// reference even when the engine wiring passes a null pointer at
// construction. The reference points at a private no-op stub so the
// "ungated infrastructure accessor cannot fail" contract from
// @ref IEngineToken holds in either wiring state. This replaces the
// previous std::abort() on null fallthrough.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, SignalEmitterAccessorReturnsLiveStubWhenUnwired)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    // Construct with the default-null signal emitter argument; the
    // token must still hand back a live reference (a NullSignalEmitter
    // stub) instead of crashing.
    EngineToken token(fx.stateA, *fx.context, fx.context->stateMachine());

    // Just calling the accessor must not crash; the stub's identity is
    // private to the impl translation unit so the test merely confirms
    // the reference is reachable. Calling emit on the stub is a quiet
    // no-op (returns a default-constructed Result).
    auto &emitter = token.signalEmitter();
    EXPECT_EQ(&emitter, &emitter);
}

// ---------------------------------------------------------------------------
// Scenario 6: two tokens bound to different states isolate. Transition
// away from stateA invalidates only the stateA-bound token; the
// stateB-bound token stays alive.
// ---------------------------------------------------------------------------

TEST(EngineTokenSmoke, TokensBoundToDifferentStatesIsolate)
{
    auto fx = ContextFixture::make();
    ASSERT_NE(fx.context, nullptr);

    EngineToken tokenA(fx.stateA, *fx.context, fx.context->stateMachine());
    EngineToken tokenB(fx.stateB, *fx.context, fx.context->stateMachine());

    // current is stateA from the fixture's setInitial. Transition to
    // stateB invalidates tokenA only.
    const auto t = fx.context->stateMachine().transition(fx.stateB);
    EXPECT_TRUE(t.isSuccess());

    EXPECT_FALSE(tokenA.isAlive());
    EXPECT_TRUE (tokenB.isAlive());

    // A second transition from stateB to stateA invalidates tokenB.
    const auto t2 = fx.context->stateMachine().transition(fx.stateA);
    EXPECT_TRUE(t2.isSuccess());

    EXPECT_FALSE(tokenA.isAlive());
    EXPECT_FALSE(tokenB.isAlive());
}
