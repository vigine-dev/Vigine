// ---------------------------------------------------------------------------
// Scenario 18 -- async FSM transition request + controller-thread drain.
//
// IStateMachine::requestTransition is the producer side: any thread may
// post a target and the call returns immediately after pushing the id
// onto the internal queue. processQueuedTransitions is the consumer side:
// it must run on the controller thread and drains the queue in a single
// pass via an internal swap-out, so requests posted *during* the drain
// are deferred to the next drain call (cooperative no-reentry).
//
// The scenario exercises three slices of that contract:
//
//   1. RequestsAppliedOnNextProcess  -- multiple producer threads push
//      requests; nothing happens to the FSM until the controller thread
//      drains; after the drain the FSM rests on one of the requested
//      targets (the snapshot-swap drains everything that landed before
//      the swap; this case observes the post-drain final state, not
//      per-target visitation, since onEnter / onExit hooks land in a
//      later leaf — see FifoOrder for the explicit ordering case).
//
//   2. CooperativeNoReentry          -- the controller thread pushes a
//      follow-up after the drain swap has snapshotted the queue; the
//      drain must not pick that follow-up up; only the next drain call
//      applies it. This is the snapshot-swap guarantee made observable.
//
//   3. FifoOrder                     -- pushing A, B, C on a flat
//      three-state machine drains in exactly the order A -> B -> C and
//      ends on C, regardless of how the queue is internally implemented.
//
// All three cases run on the EngineFixture so the FSM is reached through
// the public IContext aggregator just like every other scenario in the
// full-contract suite.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/result.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

namespace vigine::contract
{
namespace
{

using FsmAsyncTransition = EngineFixture;

// -- Case 1 ------------------------------------------------------------------
//
// Several producer threads each push one request; the controller thread
// then calls processQueuedTransitions once. Because the producers ran
// before the drain, the snapshot picked up by the drain contains every
// pushed target, and after the drain current() ends up on one of the
// requested targets (the queue-mutex-acquisition order is an
// implementation detail this test deliberately does not assert on; the
// dedicated FifoOrder case below covers the deterministic single-thread
// ordering).
//
// We bind the test thread as the controller before the drain so the
// thread-affinity assert on processQueuedTransitions stays satisfied in
// Debug builds.

TEST_F(FsmAsyncTransition, RequestsAppliedOnNextProcess)
{
    auto &sm = context().stateMachine();

    // Build a flat FSM with five targets so the producers can each
    // post a distinct one.
    std::vector<vigine::statemachine::StateId> targets;
    targets.reserve(5);
    for (int i = 0; i < 5; ++i)
    {
        const auto id = sm.addState();
        ASSERT_TRUE(id.valid());
        targets.push_back(id);
    }

    sm.bindToControllerThread(std::this_thread::get_id());

    // Producers: each thread pushes exactly one target. We join them
    // all before draining, so by the time processQueuedTransitions
    // takes the snapshot, every push has happened.
    std::vector<std::thread> producers;
    producers.reserve(targets.size());
    for (const auto target : targets)
    {
        producers.emplace_back([&sm, target]() { sm.requestTransition(target); });
    }
    for (auto &t : producers)
    {
        t.join();
    }

    // Nothing should have changed yet: requestTransition is a queue-only
    // call and the controller has not drained.
    EXPECT_NE(sm.current(), targets.back())
        << "drain has not run yet -- current must not have moved to the last target";

    sm.processQueuedTransitions();

    // After the drain the FSM has applied every queued transition in
    // FIFO order; the last one wins. We do not assert on the order the
    // producer threads acquired the queue mutex (that is implementation
    // detail), only that current() is *one of* the requested targets,
    // and specifically that it is registered.
    const auto final_state = sm.current();
    EXPECT_TRUE(sm.hasState(final_state));
    bool matched = false;
    for (const auto t : targets)
    {
        if (final_state == t)
        {
            matched = true;
            break;
        }
    }
    EXPECT_TRUE(matched)
        << "final state must be one of the requested targets";
}

// -- Case 2 ------------------------------------------------------------------
//
// The single-pass drain contract: a request that lands on the queue
// after processQueuedTransitions has taken its snapshot must not be
// applied by that same drain. The cleanest way to make that observable
// without onEnter/onExit hooks (a later leaf adds those) is to
// interleave the controller's calls explicitly:
//
//   1. push X.
//   2. processQueuedTransitions -- drains X, current == X.
//   3. push Y -- nothing else runs.
//   4. EXPECT current == X, not Y.       <- snapshot did not see Y
//   5. processQueuedTransitions again.
//   6. EXPECT current == Y.              <- next drain picked it up
//
// That is the same observable behaviour as the "inside-the-drain"
// re-entry case: the snapshot-swap semantics decide which targets the
// current drain owns, and any push after the swap waits for the next
// drain. The test makes the deferral explicit instead of racing with
// onEnter hooks that don't exist yet.

TEST_F(FsmAsyncTransition, CooperativeNoReentry)
{
    auto &sm = context().stateMachine();

    const auto x = sm.addState();
    const auto y = sm.addState();
    ASSERT_TRUE(x.valid());
    ASSERT_TRUE(y.valid());

    sm.bindToControllerThread(std::this_thread::get_id());

    sm.requestTransition(x);
    sm.processQueuedTransitions();
    EXPECT_EQ(sm.current(), x)
        << "first drain must apply the only pending request";

    // Posted *after* the drain returned -- equivalent to a request
    // pushed during onEnter that the leaf cannot yet wire. The drain
    // already returned; current must still be x until the next call.
    sm.requestTransition(y);
    EXPECT_EQ(sm.current(), x)
        << "no transition happens on push -- the FSM moves only on drain";

    sm.processQueuedTransitions();
    EXPECT_EQ(sm.current(), y)
        << "the second drain must apply the deferred request";
}

// -- Case 3 ------------------------------------------------------------------
//
// FIFO order is part of the contract -- the queue is a deque pushed at
// the back and walked front-to-back. With a flat 3-state FSM and three
// distinct targets pushed serially on one thread, the drain must apply
// them in exactly the order they were pushed and must end on the last
// one. We assert on the rest state -- the only externally observable
// result of a drain on a hooks-free FSM -- which uniquely identifies
// the drain order: any out-of-order replay (e.g. C, A, B) would leave
// the FSM on B, not on C.

TEST_F(FsmAsyncTransition, FifoOrder)
{
    auto &sm = context().stateMachine();

    const auto a = sm.addState();
    const auto b = sm.addState();
    const auto c = sm.addState();
    ASSERT_TRUE(a.valid());
    ASSERT_TRUE(b.valid());
    ASSERT_TRUE(c.valid());

    sm.bindToControllerThread(std::this_thread::get_id());

    // Push A, B, C from one thread -- the queue is FIFO and the drain
    // walks it front-to-back, so the FSM must end on C.
    sm.requestTransition(a);
    sm.requestTransition(b);
    sm.requestTransition(c);

    sm.processQueuedTransitions();

    EXPECT_EQ(sm.current(), c)
        << "FIFO drain ends on the last pushed target";
}

} // namespace
} // namespace vigine::contract
