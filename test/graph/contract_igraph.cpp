#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/graph/factory.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

// =============================================================================
// IGraph lifecycle contract suite.
//
// Exercises the IGraph public surface through a GraphFactory parameter.
// Every test in this file works against any concrete IGraph that honours
// the surface declared in include/vigine/graph/; the factory registered
// at the bottom is the engine's own createGraph, and a second concrete
// graph wires into the same suite by adding another INSTANTIATE_TEST_SUITE_P.
// =============================================================================

namespace vigine::graph::contract
{

using LifecycleContract = ContractFixture;

// -----------------------------------------------------------------------------
// Node lifecycle.
// -----------------------------------------------------------------------------

TEST_P(LifecycleContract, AddNodeReturnsValidGenerationalId)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, kind::Generic);
    EXPECT_TRUE(a.valid());
    EXPECT_NE(a.generation, 0u);
    EXPECT_EQ(graph->nodeCount(), 1u);
}

TEST_P(LifecycleContract, AddNodeReturnsDistinctIdentifiers)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const NodeId c     = addTestNode(*graph);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_EQ(graph->nodeCount(), 3u);
}

TEST_P(LifecycleContract, NodeLookupReturnsPointerAfterInsert)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph, kind::Generic);
    const INode *ptr   = graph->node(a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id(), a);
    EXPECT_EQ(ptr->kind(), kind::Generic);
}

TEST_P(LifecycleContract, NodeLookupReturnsNullForInvalidId)
{
    auto graph = makeGraph();
    EXPECT_EQ(graph->node(NodeId{}), nullptr);
    // Concoct an identifier that was never issued.
    EXPECT_EQ(graph->node(NodeId{999, 1}), nullptr);
}

TEST_P(LifecycleContract, RemoveNodeInvalidatesLookup)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const Result r     = graph->removeNode(a);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_EQ(graph->node(a), nullptr);
    EXPECT_EQ(graph->nodeCount(), 0u);
}

TEST_P(LifecycleContract, RemoveNodeReportsErrorForStaleId)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    EXPECT_TRUE(graph->removeNode(a).isSuccess());
    const Result again = graph->removeNode(a);
    EXPECT_TRUE(again.isError());
}

TEST_P(LifecycleContract, RemoveNodeWithInvalidIdReportsError)
{
    auto         graph = makeGraph();
    const Result r     = graph->removeNode(NodeId{});
    EXPECT_TRUE(r.isError());
}

// -----------------------------------------------------------------------------
// Edge lifecycle.
// -----------------------------------------------------------------------------

TEST_P(LifecycleContract, AddEdgeReturnsValidGenerationalId)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(graph->edgeCount(), 1u);
}

TEST_P(LifecycleContract, AddEdgeStoresEndpointsAndKind)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b, edge_kind::Generic);
    const IEdge *ptr   = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id(), e);
    EXPECT_EQ(ptr->from(), a);
    EXPECT_EQ(ptr->to(), b);
    EXPECT_EQ(ptr->kind(), edge_kind::Generic);
}

TEST_P(LifecycleContract, AddEdgeAcceptsArbitraryUserKinds)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b, ContractChildOfKind);
    const IEdge *ptr   = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->kind(), ContractChildOfKind);
}

TEST_P(LifecycleContract, AddEdgeCarriesPayloadRoundTrip)
{
    auto         graph   = makeGraph();
    const NodeId a       = addTestNode(*graph);
    const NodeId b       = addTestNode(*graph);
    auto         payload = std::make_unique<TestEdgeData>(0xCAFEu, 7);
    const EdgeId e       = addTestEdge(*graph, a, b, edge_kind::Generic, std::move(payload));

    const IEdge *ptr = graph->edge(e);
    ASSERT_NE(ptr, nullptr);
    const IEdgeData *data = ptr->data();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->dataTypeId(), 0xCAFEu);
    const auto *typed = static_cast<const TestEdgeData *>(data);
    EXPECT_EQ(typed->tag(), 7);
}

TEST_P(LifecycleContract, AddEdgeRejectsStaleEndpoints)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    EXPECT_TRUE(graph->removeNode(b).isSuccess());
    const EdgeId e = addTestEdge(*graph, a, b);
    EXPECT_FALSE(e.valid());
    EXPECT_EQ(graph->edgeCount(), 0u);
}

TEST_P(LifecycleContract, EdgeLookupReturnsNullForInvalidId)
{
    auto graph = makeGraph();
    EXPECT_EQ(graph->edge(EdgeId{}), nullptr);
    EXPECT_EQ(graph->edge(EdgeId{999, 1}), nullptr);
}

TEST_P(LifecycleContract, RemoveEdgeInvalidatesLookup)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    EXPECT_TRUE(graph->removeEdge(e).isSuccess());
    EXPECT_EQ(graph->edge(e), nullptr);
    EXPECT_EQ(graph->edgeCount(), 0u);
}

TEST_P(LifecycleContract, RemoveEdgeReportsErrorForStaleId)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);
    EXPECT_TRUE(graph->removeEdge(e).isSuccess());
    EXPECT_TRUE(graph->removeEdge(e).isError());
}

TEST_P(LifecycleContract, RemoveNodeCascadesIncidentEdges)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const NodeId c     = addTestNode(*graph);
    const EdgeId ab    = addTestEdge(*graph, a, b);
    const EdgeId ac    = addTestEdge(*graph, a, c);
    const EdgeId bc    = addTestEdge(*graph, b, c);
    ASSERT_EQ(graph->edgeCount(), 3u);

    const Result r = graph->removeNode(b);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_EQ(graph->edge(ab), nullptr); // b was the target
    EXPECT_EQ(graph->edge(bc), nullptr); // b was the source
    EXPECT_NE(graph->edge(ac), nullptr); // untouched
    EXPECT_EQ(graph->edgeCount(), 1u);
    EXPECT_EQ(graph->nodeCount(), 2u);
}

// -----------------------------------------------------------------------------
// Generational id safety — a removed slot's id must not alias a later one.
// -----------------------------------------------------------------------------

TEST_P(LifecycleContract, StaleNodeIdDoesNotAliasReusedSlot)
{
    auto         graph   = makeGraph();
    const NodeId firstId = addTestNode(*graph);
    ASSERT_TRUE(graph->removeNode(firstId).isSuccess());
    const NodeId secondId = addTestNode(*graph);
    // Even if the underlying slot index is reused, the stale first id
    // never resolves to the new slot.
    EXPECT_EQ(graph->node(firstId), nullptr);
    const INode *alive = graph->node(secondId);
    ASSERT_NE(alive, nullptr);
    EXPECT_NE(alive->id(), firstId);
}

TEST_P(LifecycleContract, StaleEdgeIdDoesNotAliasReusedSlot)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId first = addTestEdge(*graph, a, b);
    ASSERT_TRUE(graph->removeEdge(first).isSuccess());
    const EdgeId second = addTestEdge(*graph, a, b);
    EXPECT_EQ(graph->edge(first), nullptr);
    const IEdge *alive = graph->edge(second);
    ASSERT_NE(alive, nullptr);
    EXPECT_NE(alive->id(), first);
}

// -----------------------------------------------------------------------------
// Observability.
// -----------------------------------------------------------------------------

TEST_P(LifecycleContract, EmptyGraphReportsZeroCounts)
{
    auto graph = makeGraph();
    EXPECT_EQ(graph->nodeCount(), 0u);
    EXPECT_EQ(graph->edgeCount(), 0u);
}

TEST_P(LifecycleContract, CountsTrackMutationsExactly)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const NodeId c     = addTestNode(*graph);
    const EdgeId ab    = addTestEdge(*graph, a, b);
    const EdgeId bc    = addTestEdge(*graph, b, c);
    EXPECT_EQ(graph->nodeCount(), 3u);
    EXPECT_EQ(graph->edgeCount(), 2u);

    EXPECT_TRUE(graph->removeEdge(ab).isSuccess());
    EXPECT_EQ(graph->edgeCount(), 1u);

    EXPECT_TRUE(graph->removeNode(a).isSuccess());
    EXPECT_EQ(graph->nodeCount(), 2u);
    EXPECT_EQ(graph->edgeCount(), 1u);

    EXPECT_TRUE(graph->removeNode(c).isSuccess());
    EXPECT_EQ(graph->nodeCount(), 1u);
    EXPECT_EQ(graph->edgeCount(), 0u);

    static_cast<void>(bc);
}

// -----------------------------------------------------------------------------
// DOT export round-trip.
// -----------------------------------------------------------------------------

TEST_P(LifecycleContract, ExportGraphVizEmitsDigraphWrapper)
{
    auto graph = makeGraph();
    std::string dot;
    const Result r = graph->exportGraphViz(dot);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_NE(dot.find("digraph"), std::string::npos);
    EXPECT_NE(dot.find("{"), std::string::npos);
    EXPECT_NE(dot.find("}"), std::string::npos);
}

TEST_P(LifecycleContract, ExportGraphVizContainsArrowsForEachEdge)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const NodeId c     = addTestNode(*graph);
    static_cast<void>(addTestEdge(*graph, a, b));
    static_cast<void>(addTestEdge(*graph, b, c));

    std::string dot;
    ASSERT_TRUE(graph->exportGraphViz(dot).isSuccess());

    std::size_t count = 0;
    std::size_t pos   = 0;
    while ((pos = dot.find("->", pos)) != std::string::npos)
    {
        ++count;
        pos += 2;
    }
    EXPECT_EQ(count, 2u);
}

TEST_P(LifecycleContract, ExportGraphVizOverwritesExistingBuffer)
{
    auto        graph = makeGraph();
    std::string dot   = "residual content that must be dropped";
    EXPECT_TRUE(graph->exportGraphViz(dot).isSuccess());
    EXPECT_EQ(dot.find("residual"), std::string::npos);
    EXPECT_NE(dot.find("digraph"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Factory registration. The single instantiation wires the suite up against
// the engine's createGraph(); a second concrete IGraph plugs in by adding
// another INSTANTIATE_TEST_SUITE_P block referencing the same LifecycleContract.
// -----------------------------------------------------------------------------

INSTANTIATE_TEST_SUITE_P(contract_igraph,
                         LifecycleContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::graph::contract
