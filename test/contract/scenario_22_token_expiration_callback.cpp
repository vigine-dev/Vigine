// ---------------------------------------------------------------------------
// Scenario 22 -- IEngineToken::subscribeExpiration callback contract.
//
// IEngineToken hands tasks a state-scoped handle on the engine API. The
// expiration-callback hook gives a task a single, well-defined moment to
// flush per-state work just before the FSM walks away from the state the
// token was bound to. The contract this scenario pins down has four
// observable slices:
//
//   1. ExactlyOnce              -- a single subscriber registered before
//      the transition fires exactly once on the first transition out of
//      the bound state and does NOT re-fire on any subsequent transition
//      (the "fired-once latch" published under the registry mutex on the
//      token impl).
//
//   2. MultipleSubscribersAllFire
//                               -- every active subscriber on the same
//      token gets exactly one callback invocation on the same transition.
//      The walk over the snapshotted callback list visits every live
//      slot once; cancelled slots are skipped.
//
//   3. FiresOnControllerThread  -- the callback runs on the thread that
//      executed the FSM transition (the controller thread, by the
//      IStateMachine thread-affinity contract). The emission point sits
//      on the AbstractStateMachine listener firing path BEFORE the
//      transition's onExit handler runs (the token impl's onStateInvalidated
//      fires the callbacks, then markExpired flips the alive flag), per
//      the emission point locked in #287.
//
//   4. NoopTransitionDoesNotFire
//                               -- a request to transition to the SAME
//      state (target == current) is short-circuited by the FSM and the
//      invalidation listener does not fire. Tokens stay alive and
//      callbacks stay un-invoked.
//
// All cases run on the EngineFixture so the FSM is reached through the
// public IContext aggregator just like every other scenario in the
// full-contract suite. The token is constructed by hand (rather than
// through IContext::makeEngineToken) so the scenario stays decoupled
// from the factory wiring -- the contract under test is the
// subscribeExpiration callback semantics, not the construction path.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/impl/engine/enginetoken.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace vigine::contract
{
namespace
{

using TokenExpirationCallback = EngineFixture;

// Build two states, latch the FSM on stateA, and return the pair so each
// case can construct an EngineToken bound to stateA and observe the
// transition stateA -> stateB. Done as a free helper instead of a fixture
// override to keep every test self-contained when reading the file
// top-to-bottom.
struct StatePair
{
    vigine::statemachine::StateId stateA;
    vigine::statemachine::StateId stateB;
};

StatePair latchOnStateA(vigine::statemachine::IStateMachine &sm)
{
    StatePair pair{};
    pair.stateA = sm.addState();
    pair.stateB = sm.addState();
    EXPECT_TRUE(pair.stateA.valid());
    EXPECT_TRUE(pair.stateB.valid());
    const auto si = sm.setInitial(pair.stateA);
    EXPECT_TRUE(si.isSuccess());
    return pair;
}

// -- Case 1 ------------------------------------------------------------------
//
// A single subscriber registered against a live token fires exactly once
// on the first FSM transition out of the bound state. A second transition
// (stateB -> stateA) happens-after the first and must NOT re-fire the
// callback: the impl's @c _expirationFired latch is set under the
// registry mutex during the first invalidation and observed raised by
// the second invalidation, which short-circuits without invoking
// anything. The "exactly once" wording in the IEngineToken docstring is
// the contract we pin down here.

TEST_F(TokenExpirationCallback, ExactlyOnce)
{
    auto &sm     = context().stateMachine();
    auto  states = latchOnStateA(sm);

    vigine::engine::EngineToken token(states.stateA, context(), sm);

    std::atomic<int> fireCount{0};
    auto sub = token.subscribeExpiration([&]() { fireCount.fetch_add(1); });
    ASSERT_NE(sub, nullptr);
    EXPECT_TRUE(sub->active());
    EXPECT_EQ(fireCount.load(), 0)
        << "callback must not fire on registration";

    // First transition out of the bound state: callback fires exactly once.
    const auto t1 = sm.transition(states.stateB);
    ASSERT_TRUE(t1.isSuccess());
    EXPECT_EQ(fireCount.load(), 1)
        << "subscribeExpiration must fire exactly once on the first "
           "transition out of the bound state";
    EXPECT_FALSE(token.isAlive())
        << "token must report expired after the invalidation";

    // Second transition (stateB -> stateA): the token's bound state was
    // stateA, but the latch already fired and the alive flag is already
    // false. The contract is "exactly once"; this transition must not
    // produce a second callback invocation.
    const auto t2 = sm.transition(states.stateA);
    ASSERT_TRUE(t2.isSuccess());
    EXPECT_EQ(fireCount.load(), 1)
        << "no second invocation after the fired-once latch is raised";
}

// -- Case 2 ------------------------------------------------------------------
//
// Every active subscriber registered against the same token receives
// exactly one callback invocation on the same FSM transition. The
// invalidation listener walks the snapshotted callback list once, so
// every live slot fires; cancelled slots (empty callback) are skipped.
// We register three subscribers, cancel one before the transition, and
// assert that only the two live ones fire.

TEST_F(TokenExpirationCallback, MultipleSubscribersAllFire)
{
    auto &sm     = context().stateMachine();
    auto  states = latchOnStateA(sm);

    vigine::engine::EngineToken token(states.stateA, context(), sm);

    std::atomic<int> firedA{0};
    std::atomic<int> firedB{0};
    std::atomic<int> firedC{0};

    auto subA = token.subscribeExpiration([&]() { firedA.fetch_add(1); });
    auto subB = token.subscribeExpiration([&]() { firedB.fetch_add(1); });
    auto subC = token.subscribeExpiration([&]() { firedC.fetch_add(1); });
    ASSERT_NE(subA, nullptr);
    ASSERT_NE(subB, nullptr);
    ASSERT_NE(subC, nullptr);

    // Cancel the middle slot before the transition fires the listener.
    // Cancelled slots have an empty callback and the firing path skips
    // them, so we expect firedB to stay at zero while firedA and firedC
    // each tick to one.
    subB->cancel();
    EXPECT_FALSE(subB->active());

    const auto t = sm.transition(states.stateB);
    ASSERT_TRUE(t.isSuccess());

    EXPECT_EQ(firedA.load(), 1)
        << "every live subscriber must fire on the bound-state transition";
    EXPECT_EQ(firedB.load(), 0)
        << "cancelled slots must not fire";
    EXPECT_EQ(firedC.load(), 1)
        << "every live subscriber must fire on the bound-state transition";
}

// -- Case 3 ------------------------------------------------------------------
//
// The IEngineToken docstring locks the emission point on the controller
// thread: "The engine invokes @p callback exactly once when the bound
// state transitions away. The concrete implementation in
// @ref vigine::engine::EngineToken runs the callback synchronously on
// whichever thread executed the FSM transition (the controller thread,
// by the @ref vigine::statemachine::IStateMachine thread-affinity
// contract)." We bind the FSM to the test thread as the controller and
// drive the transition from a producer thread + a controller-thread
// drain, and assert the captured thread id matches the controller.
//
// The test also asserts the emission ORDERING with respect to the
// alive-flag flip: the token impl runs the callbacks BEFORE markExpired,
// so a callback that re-reads isAlive() observes the live state. This
// pins down the "before onExit of the leaving state" wording in the
// scenario scope (the AbstractStateMachine listener firing path runs
// the listeners synchronously; the impl orders fire-then-flip; the
// contract is observable through this re-entrant read).

TEST_F(TokenExpirationCallback, FiresOnControllerThread)
{
    auto &sm = context().stateMachine();

    // Bind the FSM controller binding to the test thread BEFORE the
    // first sync mutation (latchOnStateA calls addState / setInitial,
    // both gated by checkThreadAffinity). The IStateMachine contract is
    // explicit: bindToControllerThread is one-shot and must be called
    // before the first sync mutation. processQueuedTransitions then
    // runs on this thread and the listener fires on this thread by the
    // FSM thread-affinity contract.
    const auto controllerId = std::this_thread::get_id();
    sm.bindToControllerThread(controllerId);

    auto states = latchOnStateA(sm);

    vigine::engine::EngineToken token(states.stateA, context(), sm);

    std::atomic<bool>            captured{false};
    std::atomic<bool>            sawAlive{false};
    std::thread::id              callbackThread{};

    auto sub = token.subscribeExpiration(
        [&]()
        {
            // Capture the thread the listener fires on so the test
            // thread can compare it against the controller binding.
            callbackThread = std::this_thread::get_id();
            // The impl fires callbacks BEFORE markExpired, so a
            // callback that reads isAlive() observes the live state.
            // This is the "before onExit" emission ordering that
            // scenario_22 is meant to pin down.
            sawAlive.store(token.isAlive(), std::memory_order_release);
            captured.store(true, std::memory_order_release);
        });
    ASSERT_NE(sub, nullptr);

    // Drive the transition from a producer thread that posts on the
    // queue; the controller-thread drain then runs the listener on the
    // controller thread. We join the producer first to make the snapshot
    // pickup deterministic before the drain.
    std::thread producer(
        [&sm, target = states.stateB]()
        {
            sm.requestTransition(target);
        });
    producer.join();

    // Drain on the test thread (controller). The listener fires here.
    sm.processQueuedTransitions();

    ASSERT_TRUE(captured.load(std::memory_order_acquire))
        << "callback must have fired by the time the drain returns";
    EXPECT_EQ(callbackThread, controllerId)
        << "callback must run on the FSM controller thread";
    EXPECT_TRUE(sawAlive.load(std::memory_order_acquire))
        << "callback must run BEFORE markExpired flips the alive flag "
           "(emission point locked in #287)";

    // After the drain returns, the alive flag is flipped and the gated
    // accessors short-circuit. Assert this so the ordering test does
    // not silently pass against a future impl that flips the flag
    // before firing.
    EXPECT_FALSE(token.isAlive());
}

// -- Case 4 ------------------------------------------------------------------
//
// A no-op transition is one where the requested target equals the
// FSM's current state. The IStateMachine implementation short-circuits
// without firing the invalidation listener, so the bound token stays
// alive and any subscribed callback stays un-invoked. This is the
// "target == current => no-op" branch of the FSM contract.

TEST_F(TokenExpirationCallback, NoopTransitionDoesNotFire)
{
    auto &sm     = context().stateMachine();
    auto  states = latchOnStateA(sm);

    vigine::engine::EngineToken token(states.stateA, context(), sm);

    std::atomic<int> fireCount{0};
    auto sub = token.subscribeExpiration([&]() { fireCount.fetch_add(1); });
    ASSERT_NE(sub, nullptr);
    EXPECT_TRUE(sub->active());

    // Transition to the SAME state the FSM already rests on. The FSM
    // short-circuits this case before reaching the invalidation
    // listener, so the listener does not fire and the token stays
    // alive.
    const auto t = sm.transition(states.stateA);
    ASSERT_TRUE(t.isSuccess())
        << "a no-op transition is not an error -- the FSM accepts it "
           "and short-circuits";

    EXPECT_TRUE(token.isAlive())
        << "no-op transition must not flip the alive flag";
    EXPECT_EQ(fireCount.load(), 0)
        << "no-op transition must not fire the expiration callback";

    // Driving a real transition stateA -> stateB after the no-op MUST
    // still fire the callback. This guards against a future regression
    // where a prior no-op accidentally raises the fired-once latch.
    const auto t2 = sm.transition(states.stateB);
    ASSERT_TRUE(t2.isSuccess());
    EXPECT_EQ(fireCount.load(), 1)
        << "the next real transition out of the bound state must still "
           "fire the callback after a prior no-op";
}

} // namespace
} // namespace vigine::contract
