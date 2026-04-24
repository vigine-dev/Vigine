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

#include <atomic>
#include <cstddef>
#include <unordered_set>
#include <vector>

// =============================================================================
// Scenario l — FanOut activation.
//
// A FanOut wrapper activates every direct subscriber of a target node and
// the graph substrate must let the visitor enumerate those subscribers in
// a single BFS hop, honouring a kind filter. Parallel dispatch of the
// callbacks is the wrapper's responsibility; at the graph level the
// contract is that every direct subscriber is reported exactly once and
// the walk does not descend past one level when the visitor returns Skip
// at depth one.
// =============================================================================

namespace vigine::core::graph::contract
{
namespace
{

struct NodeIdHashPolicy
{
    std::size_t operator()(NodeId id) const noexcept
    {
        return (static_cast<std::size_t>(id.index) << 32)
               ^ static_cast<std::size_t>(id.generation);
    }
};

// FanOut visitor — visits the seed, then Skip-prunes every depth-1 child
// so BFS never descends past the first neighbourhood. Counts visits so
// the test can assert the exact set of subscribers observed.
class FanOutVisitor : public IGraphVisitor
{
  public:
    explicit FanOutVisitor(NodeId seed) noexcept : _seed(seed) {}

    VisitResult onNode(const INode &node) override
    {
        _visited.push_back(node.id());
        // Prune every depth-1 child so the walk does not recurse into
        // grandchildren; FanOut is one level deep by definition.
        return node.id() == _seed ? VisitResult::Continue : VisitResult::Skip;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] const std::vector<NodeId> &visited() const { return _visited; }

  private:
    NodeId              _seed;
    std::vector<NodeId> _visited;
};

} // namespace

using FanOutContract = ContractFixture;

TEST_P(FanOutContract, VisitsEveryDirectSubscriberExactlyOnce)
{
    auto         graph = makeGraph();
    const NodeId target = addTestNode(*graph, ContractStateKind);

    // Attach five subscribers — four Attached-kind and one Generic to
    // confirm the BFS visits every direct child regardless of kind.
    constexpr std::size_t kSubscribers = 5;
    std::vector<NodeId>   subs;
    subs.reserve(kSubscribers);
    for (std::size_t i = 0; i < kSubscribers; ++i)
    {
        const NodeId s = addTestNode(*graph, ContractSubscriberKind);
        subs.push_back(s);
        const EdgeKind k = (i + 1 == kSubscribers) ? edge_kind::Generic : ContractAttachedKind;
        addTestEdge(*graph, target, s, k);
    }

    FanOutVisitor visitor(target);
    const Result  r = graph->traverse(target, TraverseMode::BreadthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());

    // The visitor records the seed plus each direct subscriber exactly
    // once. Total = kSubscribers + 1.
    EXPECT_EQ(visitor.visited().size(), kSubscribers + 1);

    std::unordered_set<NodeId, NodeIdHashPolicy> observed(
        visitor.visited().begin(), visitor.visited().end());
    EXPECT_EQ(observed.size(), visitor.visited().size());
    EXPECT_TRUE(observed.count(target));
    for (NodeId s : subs)
    {
        EXPECT_TRUE(observed.count(s)) << "subscriber " << s.index << " not visited";
    }
}

TEST_P(FanOutContract, SkipAtDepthOneHaltsDescent)
{
    auto         graph  = makeGraph();
    const NodeId target = addTestNode(*graph, ContractStateKind);

    const NodeId childA     = addTestNode(*graph, ContractSubscriberKind);
    const NodeId childB     = addTestNode(*graph, ContractSubscriberKind);
    const NodeId grandchild = addTestNode(*graph, ContractSubscriberKind);

    addTestEdge(*graph, target, childA);
    addTestEdge(*graph, target, childB);
    addTestEdge(*graph, childA, grandchild);
    addTestEdge(*graph, childB, grandchild);

    FanOutVisitor visitor(target);
    const Result  r = graph->traverse(target, TraverseMode::BreadthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());

    std::unordered_set<NodeId, NodeIdHashPolicy> observed(
        visitor.visited().begin(), visitor.visited().end());
    EXPECT_TRUE(observed.count(target));
    EXPECT_TRUE(observed.count(childA));
    EXPECT_TRUE(observed.count(childB));
    // The grandchild is two hops away and Skip prunes the descent.
    EXPECT_FALSE(observed.count(grandchild));
}

INSTANTIATE_TEST_SUITE_P(contract_fanout,
                         FanOutContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
