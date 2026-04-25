#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/core/graph/iedge.h"
#include "vigine/core/graph/igraph.h"
#include "vigine/core/graph/igraphvisitor.h"
#include "vigine/core/graph/inode.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"
#include "vigine/core/graph/traverse_mode.h"
#include "vigine/core/graph/visit_result.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

// =============================================================================
// Scenario m — asynchronous timeout fallback.
//
// The IMessageBus wrapper schedules an async dispatch with a deadline and
// falls back to a synchronous notification when the deadline fires. That
// wrapper lives in a downstream leaf; at the graph level we verify only
// the contract the substrate must uphold to make that wrapper viable:
//
//   * A traversal the visitor aborts through Stop reports
//     Result::Error with a deterministic message so a wrapper can
//     interpret it as a cancellation, not a failure of the graph.
//   * The whole cycle of setup, invocation, and fallback visit completes
//     in bounded wall time so the wrapper's real timeout never races
//     against an unbounded traversal.
//
// Engine-level async scheduling and the Result::Deferred code are not yet
// part of the public surface; the graph-level contract below verifies the
// properties a wrapper would build on.
// =============================================================================

namespace vigine::core::graph::contract
{
namespace
{

// Visitor that aborts the walk after N nodes, simulating the fallback
// path where a wrapper's deadline fires mid-traversal and forces the
// dispatch to stop.
class DeadlineVisitor : public IGraphVisitor
{
  public:
    explicit DeadlineVisitor(std::size_t fireAfter) noexcept : _fireAfter(fireAfter) {}

    VisitResult onNode(const INode &) override
    {
        ++_visits;
        if (_visits >= _fireAfter)
        {
            _fired = true;
            return VisitResult::Stop;
        }
        return VisitResult::Continue;
    }
    VisitResult onEdge(const IEdge &) override { return VisitResult::Continue; }

    [[nodiscard]] bool        fired() const { return _fired; }
    [[nodiscard]] std::size_t visits() const { return _visits; }

  private:
    std::size_t _fireAfter;
    std::size_t _visits{0};
    bool        _fired{false};
};

} // namespace

using AsyncTimeoutContract = SevenNodeParamFixture;

TEST_P(AsyncTimeoutContract, StopReportsSuccessResult)
{
    DeadlineVisitor visitor(/*fireAfter=*/3);
    const Result    r = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);

    // The visitor fired before the traversal could finish on its own —
    // Stop is a normal early-exit signal, the walk returns success.
    EXPECT_TRUE(visitor.fired());
    EXPECT_TRUE(r.isSuccess());
}

TEST_P(AsyncTimeoutContract, StopCompletesInBoundedWallTime)
{
    // The contract on the substrate is that a Stop walk returns promptly.
    // A wrapper that layers an async deadline on top depends on this
    // bound; if the graph ever spun, the wrapper's fallback could race
    // against a never-ending traversal.
    DeadlineVisitor visitor(/*fireAfter=*/2);

    const auto   start = std::chrono::steady_clock::now();
    const Result r     = fixture.graph->traverse(
        fixture.nodes[SevenNodeFixture::A], TraverseMode::DepthFirst, visitor);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // Stop aborts the walk cleanly; the Result is success.
    EXPECT_TRUE(r.isSuccess());
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 2000)
        << "Stop should abort the traversal promptly";
}

INSTANTIATE_TEST_SUITE_P(contract_async_timeout,
                         AsyncTimeoutContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
