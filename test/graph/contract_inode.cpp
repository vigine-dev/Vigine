#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/core/graph/inode.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"

#include <gtest/gtest.h>

#include <type_traits>

// =============================================================================
// INode contract suite.
//
// Focus: id() / kind() stability once a concrete INode is owned by an
// IGraph, plus the type-level pinning guarantee (copy / move deleted).
// =============================================================================

namespace vigine::core::graph::contract
{

using NodeContract = ContractFixture;

TEST_P(NodeContract, KindMatchesConstructionValue)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, kind::Generic);
    const INode *ptr   = graph->node(a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->kind(), kind::Generic);
}

TEST_P(NodeContract, KindIsImmutableAcrossLookups)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, ContractStateKind);
    const INode *first = graph->node(a);
    ASSERT_NE(first, nullptr);
    const NodeKind k = first->kind();

    // Look up again; kind must not have shifted.
    const INode *second = graph->node(a);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->kind(), k);
}

TEST_P(NodeContract, IdMatchesAssignedValueAfterAddNode)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, kind::Generic);
    const INode *ptr   = graph->node(a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id(), a);
}

TEST_P(NodeContract, KindSurvivesOtherNodesMutations)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, ContractSubscriberKind);
    const NodeId b     = addTestNode(*graph, kind::Generic);
    const NodeId c     = addTestNode(*graph, ContractStateKind);

    static_cast<void>(b);
    EXPECT_TRUE(graph->removeNode(c).isSuccess());

    const INode *ptr = graph->node(a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->kind(), ContractSubscriberKind);
    EXPECT_EQ(ptr->id(), a);
}

// INode is pinned at the type level: copy and move operations are deleted.
// The concrete TestNode used by the suite inherits that pinning, which the
// compile-time traits below verify.
static_assert(!std::is_copy_constructible_v<INode>,
              "INode must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<INode>,
              "INode must not be copy-assignable");
static_assert(!std::is_move_constructible_v<INode>,
              "INode must not be move-constructible");
static_assert(!std::is_move_assignable_v<INode>,
              "INode must not be move-assignable");

INSTANTIATE_TEST_SUITE_P(contract_inode,
                         NodeContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
