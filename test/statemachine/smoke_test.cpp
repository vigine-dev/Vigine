// ---------------------------------------------------------------------------
// State-machine smoke suite.
//
// Exercises the IStateMachine wrapper directly via createStateMachine().
// Scenarios cover the contract every concrete machine must honour:
//   * Default state is auto-provisioned and is the initial current.
//   * addState / setInitial / transition happy paths.
//   * setInitial / transition reject stale ids with Result::Error.
//   * Hierarchy: addChildState + parent + isAncestorOf round trip.
//   * Hierarchy invariants: single-parent enforcement and cycle
//     rejection at the `addChildState` boundary.
//   * RouteMode getter/setter reflects the caller's selection.
// ---------------------------------------------------------------------------

#include "vigine/result.h"
#include "vigine/statemachine/factory.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/routemode.h"
#include "vigine/statemachine/stateid.h"

#include <gtest/gtest.h>

#include <memory>

using vigine::Result;
using vigine::statemachine::createStateMachine;
using vigine::statemachine::IStateMachine;
using vigine::statemachine::RouteMode;
using vigine::statemachine::StateId;

TEST(StateMachineSmoke, DefaultStateIsRegisteredAndCurrent)
{
    auto sm = createStateMachine();
    ASSERT_NE(sm, nullptr);

    // The constructor auto-provisions one default state and selects
    // it as the initial current. The id is valid.
    const StateId initial = sm->current();
    EXPECT_TRUE(initial.valid());
    EXPECT_TRUE(sm->hasState(initial));
}

TEST(StateMachineSmoke, AddStateSetInitialTransitionRoundTrip)
{
    auto sm = createStateMachine();

    const StateId a = sm->addState();
    const StateId b = sm->addState();
    ASSERT_TRUE(a.valid());
    ASSERT_TRUE(b.valid());
    ASSERT_NE(a, b);

    // setInitial on a valid id succeeds and flips current.
    const Result si = sm->setInitial(a);
    EXPECT_TRUE(si.isSuccess());
    EXPECT_EQ(sm->current(), a);

    // transition on a valid id succeeds and flips current.
    const Result t = sm->transition(b);
    EXPECT_TRUE(t.isSuccess());
    EXPECT_EQ(sm->current(), b);
}

TEST(StateMachineSmoke, TransitionOnStaleIdErrorsWithoutSideEffects)
{
    auto sm = createStateMachine();

    const StateId a = sm->addState();
    ASSERT_TRUE(a.valid());

    const StateId stale{42, 42};
    const Result r = sm->transition(stale);
    EXPECT_TRUE(r.isError());

    // current stays on whatever was active before the failed transition
    // — definitely not on the stale id.
    EXPECT_NE(sm->current(), stale);
}

TEST(StateMachineSmoke, HierarchyParentAndAncestorOf)
{
    auto sm = createStateMachine();

    const StateId root   = sm->addState();
    const StateId mid    = sm->addState();
    const StateId leaf   = sm->addState();

    ASSERT_TRUE(sm->addChildState(root, mid).isSuccess());
    ASSERT_TRUE(sm->addChildState(mid,  leaf).isSuccess());

    EXPECT_EQ(sm->parent(leaf), mid);
    EXPECT_EQ(sm->parent(mid),  root);
    EXPECT_FALSE(sm->parent(root).valid());

    EXPECT_TRUE (sm->isAncestorOf(root, leaf));
    EXPECT_TRUE (sm->isAncestorOf(mid,  leaf));
    EXPECT_FALSE(sm->isAncestorOf(leaf, root));
    EXPECT_FALSE(sm->isAncestorOf(root, root));  // strict relation
}

TEST(StateMachineSmoke, SingleParentInvariantRejected)
{
    auto sm = createStateMachine();

    const StateId first  = sm->addState();
    const StateId second = sm->addState();
    const StateId child  = sm->addState();

    ASSERT_TRUE(sm->addChildState(first, child).isSuccess());

    // Second attempt to reparent the same child is rejected — a
    // state cannot have two parents in the hierarchy.
    const Result dup = sm->addChildState(second, child);
    EXPECT_TRUE(dup.isError());
    EXPECT_EQ(sm->parent(child), first);
}

TEST(StateMachineSmoke, CycleRejectedAtAddChildBoundary)
{
    auto sm = createStateMachine();

    const StateId a = sm->addState();
    const StateId b = sm->addState();

    ASSERT_TRUE(sm->addChildState(a, b).isSuccess());

    // A -> B already. Attempting to also wire B -> A introduces a
    // cycle (A would become its own ancestor) — reject.
    const Result cycle = sm->addChildState(b, a);
    EXPECT_TRUE(cycle.isError());
}

TEST(StateMachineSmoke, RouteModeGetterSetterRoundTrip)
{
    auto sm = createStateMachine();

    // Default route mode is Bubble.
    EXPECT_EQ(sm->routeMode(), RouteMode::Bubble);

    sm->setRouteMode(RouteMode::Direct);
    EXPECT_EQ(sm->routeMode(), RouteMode::Direct);

    sm->setRouteMode(RouteMode::Broadcast);
    EXPECT_EQ(sm->routeMode(), RouteMode::Broadcast);
}
