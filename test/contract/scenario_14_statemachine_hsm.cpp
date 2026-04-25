// ---------------------------------------------------------------------------
// Scenario 14 -- state machine HSM parent-chain walk.
//
// The state-machine wrapper provides hierarchical parent/child edges and
// transitions between states. The scenario:
//
//   1. Registers four states: root, mid1, mid2, leaf.
//   2. Wires leaf -> mid1 -> mid2 -> root via addChildState.
//   3. Asserts isAncestorOf(root, leaf) is true and parent(leaf) == mid1.
//   4. Calls setInitial(leaf); transition(mid1); verifies current()
//      reflects the latest transition.
//
// The full bubble-route behaviour (message delivery from the active
// state up the parent chain until a handler consumes the message) is
// wired in a later leaf that connects the state machine to the bus.
// This scenario exercises the hierarchy API only -- the observable
// surface of IStateMachine in the current codebase.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/result.h"
#include "vigine/api/statemachine/istatemachine.h"

#include <gtest/gtest.h>

namespace vigine::contract
{
namespace
{

using StateMachineHsm = EngineFixture;

TEST_F(StateMachineHsm, HierarchyAncestorAndTransition)
{
    auto &sm = context().stateMachine();

    const auto root = sm.addState();
    const auto mid2 = sm.addState();
    const auto mid1 = sm.addState();
    const auto leaf = sm.addState();

    ASSERT_TRUE(root.valid());
    ASSERT_TRUE(mid2.valid());
    ASSERT_TRUE(mid1.valid());
    ASSERT_TRUE(leaf.valid());

    // addChildState(parent, child) wires child -> parent in the ChildOf
    // topology, so parent(child) returns the supplied parent later.
    EXPECT_TRUE(sm.addChildState(root, mid2).isSuccess());
    EXPECT_TRUE(sm.addChildState(mid2, mid1).isSuccess());
    EXPECT_TRUE(sm.addChildState(mid1, leaf).isSuccess());

    EXPECT_EQ(sm.parent(leaf), mid1);
    EXPECT_EQ(sm.parent(mid1), mid2);
    EXPECT_EQ(sm.parent(mid2), root);
    EXPECT_FALSE(sm.parent(root).valid())
        << "root has no parent in the ChildOf chain";

    EXPECT_TRUE(sm.isAncestorOf(root, leaf))
        << "root must be reachable from leaf via the parent chain";
    EXPECT_TRUE(sm.isAncestorOf(mid2, leaf));
    EXPECT_FALSE(sm.isAncestorOf(leaf, root))
        << "the ancestor relation is strict -- descendants are not ancestors";

    EXPECT_TRUE(sm.setInitial(leaf).isSuccess());
    EXPECT_EQ(sm.current(), leaf);

    EXPECT_TRUE(sm.transition(mid1).isSuccess());
    EXPECT_EQ(sm.current(), mid1)
        << "current must reflect the most recent transition";
}

} // namespace
} // namespace vigine::contract
