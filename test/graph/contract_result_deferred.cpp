#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/graph/iedge.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/igraphvisitor.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/nodeid.h"
#include "vigine/graph/traverse_mode.h"
#include "vigine/graph/visit_result.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <unordered_set>
#include <vector>

// =============================================================================
// Scenario n — Result::Deferred round-trip.
//
// The planner pinned a scenario where a wrapper emits Result::Deferred
// from a synchronous path and later satisfies the deferred outcome when
// an asynchronous signal arrives. The core engine's Result type carries
// only two codes (Success / Error), so "deferred" is not yet part of
// the public surface; a wrapper-level code will cover that contract once
// it lands.
//
// At the graph-substrate level the contract reduces to: a visitor that
// chooses to defer processing of a node (signalled via Skip plus an
// external queue the visitor owns) sees the graph continue walking, and
// a follow-up traversal over the deferred set completes successfully.
// Both halves together model the sync-skip-then-async-complete pattern
// a wrapper needs.
// =============================================================================

namespace vigine::graph::contract
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

// Visitor that defers (Skip) every other node and records the deferred
// set. The caller then "fires the signal" by feeding the deferred nodes
// into a second traversal.
class DeferringVisitor : public IGraphVisitor
{
  public:
    VisitResult onNode(const INode &node) override
    {
        _seen.push_back(node.id());
        ++_counter;
        if (_counter % 2 == 0)
        {
            _deferred.push_back(node.id());
            return VisitResult::Skip;
        }
        return VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] const std::vector<NodeId> &seen() const { return _seen; }
    [[nodiscard]] const std::vector<NodeId> &deferred() const { return _deferred; }

  private:
    std::vector<NodeId> _seen;
    std::vector<NodeId> _deferred;
    std::size_t         _counter{0};
};

// Visitor that resolves deferred work — the "async completion" phase.
class ResolverVisitor : public IGraphVisitor
{
  public:
    VisitResult onNode(const INode &node) override
    {
        _resolved.push_back(node.id());
        return VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] const std::vector<NodeId> &resolved() const { return _resolved; }

  private:
    std::vector<NodeId> _resolved;
};

} // namespace

using ResultDeferredContract = SevenNodeParamFixture;

TEST_P(ResultDeferredContract, SyncPhaseCollectsDeferredSet)
{
    DeferringVisitor visitor;
    const Result     r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    EXPECT_TRUE(r.isSuccess());

    // At least one node was deferred; none was deferred twice.
    EXPECT_FALSE(visitor.deferred().empty());
    std::unordered_set<NodeId, NodeIdHashPolicy> uniq(
        visitor.deferred().begin(), visitor.deferred().end());
    EXPECT_EQ(uniq.size(), visitor.deferred().size());
}

TEST_P(ResultDeferredContract, AsyncResolutionDrainsDeferredSet)
{
    DeferringVisitor sync;
    ASSERT_TRUE(fixture.graph->traverse(
                    fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, sync)
                    .isSuccess());

    // "Signal arrives" — fire a second traversal over each deferred node.
    ResolverVisitor async;
    for (NodeId deferred : sync.deferred())
    {
        const Result r = fixture.graph->traverse(
            deferred, TraverseMode::DepthFirst, async);
        EXPECT_TRUE(r.isSuccess());
    }

    // Every deferred node is visited at least once during resolution.
    std::unordered_set<NodeId, NodeIdHashPolicy> resolvedSet(
        async.resolved().begin(), async.resolved().end());
    for (NodeId deferred : sync.deferred())
    {
        EXPECT_TRUE(resolvedSet.count(deferred))
            << "deferred node " << deferred.index << " not resolved";
    }
}

INSTANTIATE_TEST_SUITE_P(contract_result_deferred,
                         ResultDeferredContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::graph::contract
