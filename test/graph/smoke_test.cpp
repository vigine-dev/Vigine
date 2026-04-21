#include "graph/defaultgraph.h"
#include "vigine/graph/abstractgraph.h"
#include "vigine/graph/factory.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/igraphvisitor.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/traverse_mode.h"
#include "vigine/graph/visit_result.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace vigine;
using namespace vigine::graph;

namespace
{
class RecordingVisitor final : public IGraphVisitor
{
  public:
    VisitResult onNode(const INode &node) override
    {
        visitedNodes.push_back(node.id());
        return VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &edge) override
    {
        visitedEdges.push_back(edge.id());
        return VisitResult::Continue;
    }

    std::vector<NodeId> visitedNodes;
    std::vector<EdgeId> visitedEdges;
};
} // namespace

TEST(DefaultGraphSmoke, CreateAndAddNodes)
{
    std::shared_ptr<IGraph> graph = createGraph();
    ASSERT_NE(graph, nullptr);

    const NodeId n1 = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId n2 = graph->addNode(internal::makeNode(kind::Generic));
    EXPECT_TRUE(n1.valid());
    EXPECT_TRUE(n2.valid());
    EXPECT_EQ(graph->nodeCount(), 2u);

    EXPECT_NE(graph->node(n1), nullptr);
    EXPECT_EQ(graph->node(n1)->kind(), kind::Generic);
    EXPECT_EQ(graph->node(n1)->id(), n1);
}

TEST(DefaultGraphSmoke, AddEdgeAndTraverseDfs)
{
    std::shared_ptr<IGraph> graph = createGraph();
    const NodeId            a     = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId            b     = graph->addNode(internal::makeNode(kind::Generic));
    const EdgeId            e     = graph->addEdge(internal::makeEdge(a, b, edge_kind::Generic));
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(graph->edgeCount(), 1u);

    RecordingVisitor visitor;
    const Result     r = graph->traverse(a, TraverseMode::DepthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());
    ASSERT_EQ(visitor.visitedNodes.size(), 2u);
    EXPECT_EQ(visitor.visitedNodes[0], a);
    EXPECT_EQ(visitor.visitedNodes[1], b);
}

TEST(DefaultGraphSmoke, RemoveNodeCascadesEdges)
{
    std::shared_ptr<IGraph> graph = createGraph();
    const NodeId            a     = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId            b     = graph->addNode(internal::makeNode(kind::Generic));
    const EdgeId            e     = graph->addEdge(internal::makeEdge(a, b, edge_kind::Generic));
    ASSERT_EQ(graph->edgeCount(), 1u);

    const Result removed = graph->removeNode(a);
    EXPECT_TRUE(removed.isSuccess());
    EXPECT_EQ(graph->nodeCount(), 1u);
    EXPECT_EQ(graph->edgeCount(), 0u);
    EXPECT_EQ(graph->edge(e), nullptr);
}

TEST(DefaultGraphSmoke, ExportGraphVizRoundTrip)
{
    std::shared_ptr<IGraph> graph = createGraph();
    const NodeId            a     = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId            b     = graph->addNode(internal::makeNode(kind::Generic));
    static_cast<void>(graph->addEdge(internal::makeEdge(a, b, edge_kind::Generic)));

    std::string  dot;
    const Result r = graph->exportGraphViz(dot);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_NE(dot.find("digraph G {"), std::string::npos);
    EXPECT_NE(dot.find("->"), std::string::npos);
}

TEST(DefaultGraphSmoke, TopologicalOrderAndCycleDetection)
{
    std::shared_ptr<IGraph> graph = createGraph();
    const NodeId            a     = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId            b     = graph->addNode(internal::makeNode(kind::Generic));
    const NodeId            c     = graph->addNode(internal::makeNode(kind::Generic));
    static_cast<void>(graph->addEdge(internal::makeEdge(a, b, edge_kind::Generic)));
    static_cast<void>(graph->addEdge(internal::makeEdge(b, c, edge_kind::Generic)));

    const auto order = graph->query().topologicalOrder();
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order->size(), 3u);

    EXPECT_FALSE(graph->query().hasCycle());

    static_cast<void>(graph->addEdge(internal::makeEdge(c, a, edge_kind::Generic)));
    EXPECT_TRUE(graph->query().hasCycle());
    EXPECT_FALSE(graph->query().topologicalOrder().has_value());
}
