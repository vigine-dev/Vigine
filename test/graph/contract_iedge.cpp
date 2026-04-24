#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/iedge.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

// =============================================================================
// IEdge contract suite.
//
// Focus: from/to/kind stability, payload optionality, behaviour under
// endpoint removal (edge must vanish cleanly), and the type-level pinning
// guarantee.
// =============================================================================

namespace vigine::core::graph::contract
{

using EdgeContract = ContractFixture;

TEST_P(EdgeContract, FromAndToReflectConstructionEndpoints)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    const IEdge *ptr   = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->from(), a);
    EXPECT_EQ(ptr->to(), b);
}

TEST_P(EdgeContract, KindMatchesConstructionValue)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b, ContractAttachedKind);
    const IEdge *ptr   = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->kind(), ContractAttachedKind);
}

TEST_P(EdgeContract, DataIsNullWhenNoPayloadProvided)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    const IEdge *ptr   = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->data(), nullptr);
}

TEST_P(EdgeContract, DataTypeIdIsStableAcrossLookups)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    auto         data  = std::make_unique<TestEdgeData>(0xBEEFu, 11);
    const EdgeId e     = addTestEdge(*graph, a, b, edge_kind::Generic, std::move(data));

    const IEdge *first = graph->edge(e);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(first->data(), nullptr);
    const std::uint32_t typeId = first->data()->dataTypeId();

    const IEdge *second = graph->edge(e);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(second->data(), nullptr);
    EXPECT_EQ(second->data()->dataTypeId(), typeId);
}

TEST_P(EdgeContract, EdgeDisappearsWhenSourceNodeRemoved)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    ASSERT_NE(graph->edge(e), nullptr);

    EXPECT_TRUE(graph->removeNode(a).isSuccess());
    EXPECT_EQ(graph->edge(e), nullptr);
}

TEST_P(EdgeContract, EdgeDisappearsWhenTargetNodeRemoved)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    ASSERT_NE(graph->edge(e), nullptr);

    EXPECT_TRUE(graph->removeNode(b).isSuccess());
    EXPECT_EQ(graph->edge(e), nullptr);
}

TEST_P(EdgeContract, DistinctEdgesBetweenSameEndpointsGetDistinctIds)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e1    = addTestEdge(*graph, a, b, edge_kind::Generic);
    const EdgeId e2    = addTestEdge(*graph, a, b, ContractAttachedKind);
    EXPECT_NE(e1, e2);
    EXPECT_NE(graph->edge(e1)->kind(), graph->edge(e2)->kind());
}

static_assert(!std::is_copy_constructible_v<IEdge>,
              "IEdge must not be copy-constructible");
static_assert(!std::is_move_constructible_v<IEdge>,
              "IEdge must not be move-constructible");

INSTANTIATE_TEST_SUITE_P(contract_iedge,
                         EdgeContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
