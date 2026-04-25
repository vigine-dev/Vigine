#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/iedge.h"
#include "vigine/core/graph/igraph.h"
#include "vigine/core/graph/igraphquery.h"
#include "vigine/core/graph/igraphvisitor.h"
#include "vigine/core/graph/inode.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"
#include "vigine/core/graph/traverse_mode.h"
#include "vigine/core/graph/visit_result.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <vector>

// =============================================================================
// Scenario k — bubble traversal via ChildOf edges.
//
// Stages a four-level chain of "state" nodes linked by ChildOf-kind edges
// (the child is the source, the parent is the target). The test then uses
// a Custom-mode traversal that picks the parent via
// IGraphQuery::outEdgesOfKind as its next-target selector, simulating the
// bubble-up a state-machine wrapper performs on a message a nested state
// does not handle.
//
// Key properties exercised:
//   * Arbitrary EdgeKind values (the plan assigns 49 to ChildOf) pass
//     through the graph unchanged; the query surface filters on them.
//   * Custom mode follows the chain exactly as the visitor decides.
//   * VisitResult::Stop returned from onNode short-circuits the walk;
//     the driver returns a successful Result — Stop is a normal
//     early-exit signal, not a fault.
// =============================================================================

namespace vigine::core::graph::contract
{
namespace
{

// Visitor that walks the ChildOf chain upward. It records every visited
// node and stops the walk when it reaches the "handler" node configured
// by the test.
class BubbleVisitor : public IGraphVisitor
{
  public:
    BubbleVisitor(const IGraph &graph, NodeId handler) noexcept
        : _graph(graph)
        , _handler(handler)
    {
    }

    VisitResult onNode(const INode &node) override
    {
        _order.push_back(node.id());
        return node.id() == _handler ? VisitResult::Stop : VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    NodeId nextForCustom(const INode &current) override
    {
        // Follow the single ChildOf-kind out-edge (child -> parent).
        const auto edges = _graph.query().outEdgesOfKind(current.id(), ContractChildOfKind);
        if (edges.empty())
        {
            return NodeId{};
        }
        const IEdge *e = _graph.edge(edges.front());
        return e ? e->to() : NodeId{};
    }

    [[nodiscard]] const std::vector<NodeId> &order() const { return _order; }

  private:
    const IGraph       &_graph;
    NodeId              _handler;
    std::vector<NodeId> _order;
};

} // namespace

using BubbleContract = ContractFixture;

TEST_P(BubbleContract, WalksChildToRootViaChildOfEdges)
{
    auto graph = makeGraph();

    // Build a four-level chain: leaf -> mid1 -> mid2 -> root.
    const NodeId leaf = addTestNode(*graph, ContractStateKind);
    const NodeId mid1 = addTestNode(*graph, ContractStateKind);
    const NodeId mid2 = addTestNode(*graph, ContractStateKind);
    const NodeId root = addTestNode(*graph, ContractStateKind);

    addTestEdge(*graph, leaf, mid1, ContractChildOfKind);
    addTestEdge(*graph, mid1, mid2, ContractChildOfKind);
    addTestEdge(*graph, mid2, root, ContractChildOfKind);

    // No handler at all — the bubble walks until the parent chain runs
    // out at the root, then the traversal completes without stopping.
    BubbleVisitor visitor(*graph, /*handler=*/NodeId{});
    const Result  r = graph->traverse(leaf, TraverseMode::Custom, visitor);
    EXPECT_TRUE(r.isSuccess());

    ASSERT_EQ(visitor.order().size(), 4u);
    EXPECT_EQ(visitor.order()[0], leaf);
    EXPECT_EQ(visitor.order()[1], mid1);
    EXPECT_EQ(visitor.order()[2], mid2);
    EXPECT_EQ(visitor.order()[3], root);
}

TEST_P(BubbleContract, HandlerNodeStopsTheBubble)
{
    auto         graph = makeGraph();
    const NodeId leaf  = addTestNode(*graph, ContractStateKind);
    const NodeId mid1  = addTestNode(*graph, ContractStateKind);
    const NodeId mid2  = addTestNode(*graph, ContractStateKind);
    const NodeId root  = addTestNode(*graph, ContractStateKind);

    addTestEdge(*graph, leaf, mid1, ContractChildOfKind);
    addTestEdge(*graph, mid1, mid2, ContractChildOfKind);
    addTestEdge(*graph, mid2, root, ContractChildOfKind);

    // Handler sits at mid2; bubble stops there, root is not visited.
    BubbleVisitor visitor(*graph, /*handler=*/mid2);
    const Result  r = graph->traverse(leaf, TraverseMode::Custom, visitor);
    // Stop is a normal early-exit signal — the walk returns success.
    EXPECT_TRUE(r.isSuccess());

    ASSERT_EQ(visitor.order().size(), 3u);
    EXPECT_EQ(visitor.order()[0], leaf);
    EXPECT_EQ(visitor.order()[1], mid1);
    EXPECT_EQ(visitor.order()[2], mid2);
    // root never visited because the handler stopped the walk.
}

TEST_P(BubbleContract, MixedKindsDoNotDisturbTheBubble)
{
    auto         graph = makeGraph();
    const NodeId leaf  = addTestNode(*graph, ContractStateKind);
    const NodeId mid   = addTestNode(*graph, ContractStateKind);
    const NodeId root  = addTestNode(*graph, ContractStateKind);

    // Bubble chain.
    addTestEdge(*graph, leaf, mid, ContractChildOfKind);
    addTestEdge(*graph, mid, root, ContractChildOfKind);

    // Noise: a Generic-kind edge from leaf to an unrelated sibling node.
    const NodeId sibling = addTestNode(*graph, ContractSubscriberKind);
    addTestEdge(*graph, leaf, sibling, edge_kind::Generic);

    BubbleVisitor visitor(*graph, /*handler=*/NodeId{});
    const Result  r = graph->traverse(leaf, TraverseMode::Custom, visitor);
    EXPECT_TRUE(r.isSuccess());

    ASSERT_EQ(visitor.order().size(), 3u);
    EXPECT_EQ(visitor.order()[0], leaf);
    EXPECT_EQ(visitor.order()[1], mid);
    EXPECT_EQ(visitor.order()[2], root);
}

INSTANTIATE_TEST_SUITE_P(contract_hsm_bubble,
                         BubbleContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
