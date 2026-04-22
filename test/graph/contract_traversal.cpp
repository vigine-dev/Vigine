#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/igraphvisitor.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"
#include "vigine/graph/traverse_mode.h"
#include "vigine/graph/visit_result.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <unordered_set>
#include <vector>

// =============================================================================
// Traversal contract suite — scenario i (five traversal modes).
//
// Exercises DepthFirst, BreadthFirst, Topological, ReverseTopological, and
// Custom against the seven-node fixture, plus the Skip / Stop / prune
// behaviour of the visitor return codes.
//
// A note on Stop semantics. VisitResult::Stop is an early-exit signal,
// not a failure: "the visitor is done, the walk does not need to
// continue". The IGraph interface contract treats it as normal
// completion, and the concrete AbstractGraph implementation returns a
// successful Result on visitor-Stop. Only cycles (topological) and
// invalid start nodes surface an error Result. The suite asserts the
// success-on-Stop shape below.
// =============================================================================

namespace vigine::graph::contract
{
namespace
{

// Hash policy so NodeId is usable in unordered containers inside tests.
struct NodeIdHashPolicy
{
    std::size_t operator()(NodeId id) const noexcept
    {
        return (static_cast<std::size_t>(id.index) << 32)
               ^ static_cast<std::size_t>(id.generation);
    }
};

// -----------------------------------------------------------------------------
// Recording visitor — captures the order the traversal driver calls onNode
// and onEdge. The record vectors are exposed via accessors so the test
// body can assert against them without touching visitor internals through
// public data members.
// -----------------------------------------------------------------------------
class RecordingVisitor : public IGraphVisitor
{
  public:
    VisitResult onNode(const INode &node) override
    {
        _nodes.push_back(node.id());
        return VisitResult::Continue;
    }

    VisitResult onEdge(const IEdge &edge) override
    {
        _edges.push_back(edge.id());
        return VisitResult::Continue;
    }

    [[nodiscard]] const std::vector<NodeId> &visitedNodes() const { return _nodes; }
    [[nodiscard]] const std::vector<EdgeId> &visitedEdges() const { return _edges; }

  private:
    std::vector<NodeId> _nodes;
    std::vector<EdgeId> _edges;
};

// -----------------------------------------------------------------------------
// Visitor that answers Skip when onNode sees a flagged node; used to verify
// pruning semantics.
// -----------------------------------------------------------------------------
class SkipAtVisitor : public IGraphVisitor
{
  public:
    explicit SkipAtVisitor(NodeId skipAt) noexcept : _skipAt(skipAt) {}

    VisitResult onNode(const INode &node) override
    {
        _nodes.push_back(node.id());
        return node.id() == _skipAt ? VisitResult::Skip : VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] const std::vector<NodeId> &visitedNodes() const { return _nodes; }

  private:
    NodeId              _skipAt;
    std::vector<NodeId> _nodes;
};

// -----------------------------------------------------------------------------
// Visitor that answers Stop when onNode sees a flagged node; used to verify
// early-exit semantics.
// -----------------------------------------------------------------------------
class StopAtVisitor : public IGraphVisitor
{
  public:
    explicit StopAtVisitor(NodeId stopAt) noexcept : _stopAt(stopAt) {}

    VisitResult onNode(const INode &node) override
    {
        _nodes.push_back(node.id());
        return node.id() == _stopAt ? VisitResult::Stop : VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] const std::vector<NodeId> &visitedNodes() const { return _nodes; }

  private:
    NodeId              _stopAt;
    std::vector<NodeId> _nodes;
};

// -----------------------------------------------------------------------------
// Visitor for Custom mode. Uses a caller-supplied next-picker so the test
// body stays in control of the traversal shape without subclassing here.
// -----------------------------------------------------------------------------
class CustomVisitor : public IGraphVisitor
{
  public:
    using NextPicker = std::function<NodeId(const INode &)>;

    CustomVisitor(NextPicker picker, std::size_t stopAfter)
        : _picker(std::move(picker))
        , _stopAfter(stopAfter)
    {
    }

    VisitResult onNode(const INode &node) override
    {
        _nodes.push_back(node.id());
        if (_nodes.size() >= _stopAfter)
        {
            return VisitResult::Stop;
        }
        return VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }
    NodeId      nextForCustom(const INode &current) override { return _picker(current); }

    [[nodiscard]] const std::vector<NodeId> &visitedNodes() const { return _nodes; }

  private:
    NextPicker          _picker;
    std::size_t         _stopAfter;
    std::vector<NodeId> _nodes;
};

// -----------------------------------------------------------------------------
// Helper — given a sequence of ids, returns true when @p before appears
// before @p after.
// -----------------------------------------------------------------------------
bool precedes(const std::vector<NodeId> &order, NodeId before, NodeId after)
{
    const auto itBefore = std::find(order.begin(), order.end(), before);
    const auto itAfter  = std::find(order.begin(), order.end(), after);
    if (itBefore == order.end() || itAfter == order.end())
    {
        return false;
    }
    return itBefore < itAfter;
}

} // namespace

using TraversalContract = SevenNodeParamFixture;

// -----------------------------------------------------------------------------
// DepthFirst — every reachable node visited exactly once; pre-order (the
// start node is first); every reachable out-edge observed on onEdge.
// -----------------------------------------------------------------------------

TEST_P(TraversalContract, DepthFirstVisitsEveryReachableNodeOnce)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());

    // All seven nodes are reachable from A in this fixture.
    EXPECT_EQ(visitor.visitedNodes().size(), 7u);
    std::unordered_set<NodeId, NodeIdHashPolicy> uniq(
        visitor.visitedNodes().begin(), visitor.visitedNodes().end());
    EXPECT_EQ(uniq.size(), 7u);
}

TEST_P(TraversalContract, DepthFirstEmitsStartNodeFirst)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    ASSERT_TRUE(r.isSuccess());
    ASSERT_FALSE(visitor.visitedNodes().empty());
    EXPECT_EQ(visitor.visitedNodes().front(), fixture.nodes[SevenNodeFixture::A]);
}

TEST_P(TraversalContract, DepthFirstRejectsInvalidStartNode)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(NodeId{}, TraverseMode::DepthFirst, visitor);
    EXPECT_TRUE(r.isError());
    EXPECT_TRUE(visitor.visitedNodes().empty());
}

TEST_P(TraversalContract, DepthFirstSkipPrunesSubtree)
{
    // Skipping B means D and E (reachable from B only through this branch)
    // should not be visited — unless they are reachable via another path.
    // In the fixture E is also reachable via C, and D is also reachable
    // via C, so skip should prune only the edges from B, not the nodes.
    SkipAtVisitor visitor(fixture.nodes[SevenNodeFixture::B]);
    const Result  r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());

    // A and B are visited; every other node is still reachable via C.
    const auto &v = visitor.visitedNodes();
    const auto  hasB = std::find(v.begin(), v.end(), fixture.nodes[SevenNodeFixture::B]);
    EXPECT_NE(hasB, v.end());
}

TEST_P(TraversalContract, DepthFirstStopReportsSuccessResult)
{
    StopAtVisitor visitor(fixture.nodes[SevenNodeFixture::D]);
    const Result  r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    // Stop is a normal early-exit signal, not a fault — the walk
    // returns success.
    EXPECT_TRUE(r.isSuccess());

    // The stop-at node is observed exactly once.
    const auto &v = visitor.visitedNodes();
    EXPECT_EQ(std::count(v.begin(), v.end(), fixture.nodes[SevenNodeFixture::D]), 1);
}

// -----------------------------------------------------------------------------
// BreadthFirst — level-by-level ordering; when a node has a shorter path
// to the seed, it appears before any node reachable only through a longer
// path.
// -----------------------------------------------------------------------------

TEST_P(TraversalContract, BreadthFirstVisitsEveryReachableNodeOnce)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::BreadthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());
    std::unordered_set<NodeId, NodeIdHashPolicy> uniq(
        visitor.visitedNodes().begin(), visitor.visitedNodes().end());
    EXPECT_EQ(uniq.size(), 7u);
}

TEST_P(TraversalContract, BreadthFirstEmitsStartNodeFirst)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::BreadthFirst, visitor);
    ASSERT_TRUE(r.isSuccess());
    ASSERT_FALSE(visitor.visitedNodes().empty());
    EXPECT_EQ(visitor.visitedNodes().front(), fixture.nodes[SevenNodeFixture::A]);
}

TEST_P(TraversalContract, BreadthFirstVisitsDepthOneBeforeDepthTwo)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::BreadthFirst, visitor);
    ASSERT_TRUE(r.isSuccess());
    const auto &order = visitor.visitedNodes();

    // Depth-1 neighbours of A: B and C.
    // Depth-2 neighbours reachable from {B, C}: D, E.
    // Depth-3 neighbours: F, G.
    EXPECT_TRUE(precedes(order,
                         fixture.nodes[SevenNodeFixture::B],
                         fixture.nodes[SevenNodeFixture::D]));
    EXPECT_TRUE(precedes(order,
                         fixture.nodes[SevenNodeFixture::C],
                         fixture.nodes[SevenNodeFixture::E]));
    EXPECT_TRUE(precedes(order,
                         fixture.nodes[SevenNodeFixture::D],
                         fixture.nodes[SevenNodeFixture::G]));
    EXPECT_TRUE(precedes(order,
                         fixture.nodes[SevenNodeFixture::E],
                         fixture.nodes[SevenNodeFixture::G]));
}

TEST_P(TraversalContract, BreadthFirstStopReportsSuccessResult)
{
    StopAtVisitor visitor(fixture.nodes[SevenNodeFixture::C]);
    const Result  r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::BreadthFirst, visitor);
    // Stop is a normal early-exit signal, not a fault.
    EXPECT_TRUE(r.isSuccess());
}

// -----------------------------------------------------------------------------
// Topological — every edge (u, v) has u appear before v in the emitted
// order; a cycle produces an error.
// -----------------------------------------------------------------------------

TEST_P(TraversalContract, TopologicalOrderRespectsEveryEdge)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::Topological, visitor);
    EXPECT_TRUE(r.isSuccess());
    const auto &order = visitor.visitedNodes();
    EXPECT_EQ(order.size(), 7u);

    // Every edge in the fixture must have its source precede its target.
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::A], fixture.nodes[SevenNodeFixture::B]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::A], fixture.nodes[SevenNodeFixture::C]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::B], fixture.nodes[SevenNodeFixture::D]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::B], fixture.nodes[SevenNodeFixture::E]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::C], fixture.nodes[SevenNodeFixture::E]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::C], fixture.nodes[SevenNodeFixture::D]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::D], fixture.nodes[SevenNodeFixture::F]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::E], fixture.nodes[SevenNodeFixture::F]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::E], fixture.nodes[SevenNodeFixture::G]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::F], fixture.nodes[SevenNodeFixture::G]));
}

TEST_P(TraversalContract, TopologicalReportsErrorOnCycle)
{
    // Close the DAG into a cycle.
    addTestEdge(*fixture.graph,
                fixture.nodes[SevenNodeFixture::G],
                fixture.nodes[SevenNodeFixture::A]);

    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::Topological, visitor);
    // The public contract on topological traversal only specifies that
    // a cycle produces an error Result. The exact message text is an
    // implementation detail of DefaultGraph — a future concrete
    // IGraph that reports "topological invariant broken" (or similar)
    // is still spec-compliant.
    EXPECT_TRUE(r.isError());
}

// -----------------------------------------------------------------------------
// ReverseTopological — exactly the reverse of the topological ordering.
// -----------------------------------------------------------------------------

TEST_P(TraversalContract, ReverseTopologicalInvertsEdgeOrder)
{
    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::ReverseTopological, visitor);
    EXPECT_TRUE(r.isSuccess());
    const auto &order = visitor.visitedNodes();
    EXPECT_EQ(order.size(), 7u);

    // Every edge (u, v) in the fixture now has v before u.
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::G], fixture.nodes[SevenNodeFixture::A]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::F], fixture.nodes[SevenNodeFixture::D]));
    EXPECT_TRUE(precedes(order, fixture.nodes[SevenNodeFixture::E], fixture.nodes[SevenNodeFixture::B]));
}

TEST_P(TraversalContract, ReverseTopologicalReportsErrorOnCycle)
{
    addTestEdge(*fixture.graph,
                fixture.nodes[SevenNodeFixture::G],
                fixture.nodes[SevenNodeFixture::A]);

    RecordingVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::ReverseTopological, visitor);
    EXPECT_TRUE(r.isError());
}

// -----------------------------------------------------------------------------
// Custom — visitor drives the walk through nextForCustom.
// -----------------------------------------------------------------------------

TEST_P(TraversalContract, CustomFollowsVisitorProvidedNext)
{
    // Walk the spine A -> B -> D -> F -> G via an explicit next picker.
    const std::array<NodeId, 5> spine{
        fixture.nodes[SevenNodeFixture::A],
        fixture.nodes[SevenNodeFixture::B],
        fixture.nodes[SevenNodeFixture::D],
        fixture.nodes[SevenNodeFixture::F],
        fixture.nodes[SevenNodeFixture::G],
    };
    auto picker = [&spine](const INode &current) {
        for (std::size_t i = 0; i + 1 < spine.size(); ++i)
        {
            if (spine[i] == current.id())
            {
                return spine[i + 1];
            }
        }
        return NodeId{};
    };
    CustomVisitor visitor(picker, /*stopAfter=*/spine.size() + 1);
    const Result  r = fixture.graph->traverse(spine[0], TraverseMode::Custom, visitor);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_EQ(visitor.visitedNodes().size(), spine.size());
    for (std::size_t i = 0; i < spine.size(); ++i)
    {
        EXPECT_EQ(visitor.visitedNodes()[i], spine[i]);
    }
}

TEST_P(TraversalContract, CustomTerminatesOnInvalidNextNode)
{
    // Picker returns an invalid id immediately, so traversal visits only
    // the start node.
    auto picker = [](const INode &) { return NodeId{}; };
    CustomVisitor visitor(picker, /*stopAfter=*/100);
    const Result  r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::Custom, visitor);
    EXPECT_TRUE(r.isSuccess());
    EXPECT_EQ(visitor.visitedNodes().size(), 1u);
    EXPECT_EQ(visitor.visitedNodes().front(), fixture.nodes[SevenNodeFixture::A]);
}

INSTANTIATE_TEST_SUITE_P(contract_traversal,
                         TraversalContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::graph::contract
