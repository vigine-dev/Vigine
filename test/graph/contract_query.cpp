#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/iedge.h"
#include "vigine/core/graph/igraph.h"
#include "vigine/core/graph/igraphquery.h"
#include "vigine/core/graph/inode.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <unordered_set>
#include <vector>

// =============================================================================
// IGraphQuery contract suite — hasNode / hasEdge, directed neighbourhood,
// kind-filtered neighbourhood, shortest path via BFS, connected components
// (undirected), hasCycle, topologicalOrder.
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

bool pathRespectsEdges(const std::vector<NodeId> &path, const IGraph &graph)
{
    if (path.empty())
    {
        return false;
    }
    for (std::size_t i = 0; i + 1 < path.size(); ++i)
    {
        const std::vector<EdgeId> outs = graph.query().outEdges(path[i]);
        bool                      found = false;
        for (EdgeId eid : outs)
        {
            const IEdge *e = graph.edge(eid);
            if (e && e->to() == path[i + 1])
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

} // namespace

using QueryContract = SevenNodeParamFixture;

// -----------------------------------------------------------------------------
// hasNode / hasEdge.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, HasNodeAgreesWithLifecycle)
{
    const IGraphQuery &q = fixture.graph->query();
    for (NodeId id : fixture.nodes)
    {
        EXPECT_TRUE(q.hasNode(id));
    }
    EXPECT_FALSE(q.hasNode(NodeId{}));
    EXPECT_FALSE(q.hasNode(NodeId{999, 1}));

    // Remove one node and re-check.
    const NodeId removed = fixture.nodes[SevenNodeFixture::A];
    ASSERT_TRUE(fixture.graph->removeNode(removed).isSuccess());
    EXPECT_FALSE(q.hasNode(removed));
}

TEST_P(QueryContract, HasEdgeAgreesWithLifecycle)
{
    const IGraphQuery &q = fixture.graph->query();
    for (EdgeId id : fixture.edges)
    {
        EXPECT_TRUE(q.hasEdge(id));
    }
    EXPECT_FALSE(q.hasEdge(EdgeId{}));
    EXPECT_FALSE(q.hasEdge(EdgeId{999, 1}));
}

// -----------------------------------------------------------------------------
// outEdges / inEdges — degree matches the fixture.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, OutEdgesDegreeMatchesFixture)
{
    const IGraphQuery &q = fixture.graph->query();
    // Fixture out-degrees: A=2, B=2, C=2, D=1, E=2, F=1, G=0.
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::A]).size(), 2u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::B]).size(), 2u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::C]).size(), 2u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::D]).size(), 1u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::E]).size(), 2u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::F]).size(), 1u);
    EXPECT_EQ(q.outEdges(fixture.nodes[SevenNodeFixture::G]).size(), 0u);
}

TEST_P(QueryContract, InEdgesDegreeMatchesFixture)
{
    const IGraphQuery &q = fixture.graph->query();
    // Fixture in-degrees: A=0, B=1, C=1, D=2, E=2, F=2, G=2.
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::A]).size(), 0u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::B]).size(), 1u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::C]).size(), 1u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::D]).size(), 2u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::E]).size(), 2u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::F]).size(), 2u);
    EXPECT_EQ(q.inEdges(fixture.nodes[SevenNodeFixture::G]).size(), 2u);
}

TEST_P(QueryContract, OutEdgesEmptyForStaleIdentifier)
{
    const IGraphQuery &q = fixture.graph->query();
    EXPECT_TRUE(q.outEdges(NodeId{999, 7}).empty());
    EXPECT_TRUE(q.inEdges(NodeId{999, 7}).empty());
}

// -----------------------------------------------------------------------------
// outEdgesOfKind / inEdgesOfKind — kind filter works.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, OutEdgesOfKindFiltersByKind)
{
    // Attach a ChildOf-kind edge from A to G — distinct from the Generic
    // edges already in the fixture.
    const EdgeId special = addTestEdge(
        *fixture.graph,
        fixture.nodes[SevenNodeFixture::A],
        fixture.nodes[SevenNodeFixture::G],
        ContractChildOfKind);
    const IGraphQuery &q = fixture.graph->query();

    const auto filtered = q.outEdgesOfKind(
        fixture.nodes[SevenNodeFixture::A], ContractChildOfKind);
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered.front(), special);

    // Generic kind still matches the two Generic edges leaving A.
    const auto generic = q.outEdgesOfKind(
        fixture.nodes[SevenNodeFixture::A], edge_kind::Generic);
    EXPECT_EQ(generic.size(), 2u);
}

TEST_P(QueryContract, InEdgesOfKindFiltersByKind)
{
    const EdgeId special = addTestEdge(
        *fixture.graph,
        fixture.nodes[SevenNodeFixture::A],
        fixture.nodes[SevenNodeFixture::G],
        ContractChildOfKind);
    const IGraphQuery &q = fixture.graph->query();

    const auto filtered = q.inEdgesOfKind(
        fixture.nodes[SevenNodeFixture::G], ContractChildOfKind);
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered.front(), special);
}

// -----------------------------------------------------------------------------
// shortestPath — unweighted BFS; trivial case; no path returns nullopt.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, ShortestPathFromNodeToItselfIsSingleton)
{
    const IGraphQuery &q    = fixture.graph->query();
    const auto         path = q.shortestPath(
        fixture.nodes[SevenNodeFixture::A], fixture.nodes[SevenNodeFixture::A]);
    ASSERT_TRUE(path.has_value());
    ASSERT_EQ(path->size(), 1u);
    EXPECT_EQ(path->front(), fixture.nodes[SevenNodeFixture::A]);
}

TEST_P(QueryContract, ShortestPathFollowsReachableEdges)
{
    const IGraphQuery &q    = fixture.graph->query();
    const auto         path = q.shortestPath(
        fixture.nodes[SevenNodeFixture::A], fixture.nodes[SevenNodeFixture::G]);
    ASSERT_TRUE(path.has_value());
    ASSERT_GE(path->size(), 2u);
    EXPECT_EQ(path->front(), fixture.nodes[SevenNodeFixture::A]);
    EXPECT_EQ(path->back(), fixture.nodes[SevenNodeFixture::G]);
    EXPECT_TRUE(pathRespectsEdges(*path, *fixture.graph));

    // In this fixture the shortest path from A to G traverses three edges
    // (A -> B -> E -> G or A -> C -> E -> G), so path size is 4.
    EXPECT_EQ(path->size(), 4u);
}

TEST_P(QueryContract, ShortestPathAbsentForUnreachableTarget)
{
    // Insert an orphan node with no incoming edges from the fixture.
    const NodeId      orphan = addTestNode(*fixture.graph);
    const IGraphQuery &q      = fixture.graph->query();
    const auto         path   = q.shortestPath(
        fixture.nodes[SevenNodeFixture::A], orphan);
    EXPECT_FALSE(path.has_value());
}

TEST_P(QueryContract, ShortestPathAbsentForStaleEndpoint)
{
    const IGraphQuery &q    = fixture.graph->query();
    const auto         path = q.shortestPath(
        fixture.nodes[SevenNodeFixture::A], NodeId{999, 9});
    EXPECT_FALSE(path.has_value());
}

// -----------------------------------------------------------------------------
// connectedComponents — the fixture is one connected shape (undirected);
// adding an orphan splits it into two components.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, ConnectedComponentsCoversEverySevenFixtureNode)
{
    const IGraphQuery &q    = fixture.graph->query();
    const auto         comps = q.connectedComponents();
    ASSERT_EQ(comps.size(), 1u);
    EXPECT_EQ(comps.front().size(), 7u);
}

TEST_P(QueryContract, ConnectedComponentsSeparatesOrphan)
{
    addTestNode(*fixture.graph);
    const IGraphQuery &q    = fixture.graph->query();
    const auto         comps = q.connectedComponents();
    EXPECT_EQ(comps.size(), 2u);

    std::size_t total = 0;
    for (const auto &c : comps)
    {
        total += c.size();
    }
    EXPECT_EQ(total, 8u);
}

// -----------------------------------------------------------------------------
// hasCycle / topologicalOrder.
// -----------------------------------------------------------------------------

TEST_P(QueryContract, AcyclicFixtureReportsNoCycle)
{
    EXPECT_FALSE(fixture.graph->query().hasCycle());
}

TEST_P(QueryContract, BackEdgeMakesCycleDetected)
{
    addTestEdge(*fixture.graph,
                fixture.nodes[SevenNodeFixture::G],
                fixture.nodes[SevenNodeFixture::A]);
    EXPECT_TRUE(fixture.graph->query().hasCycle());
}

TEST_P(QueryContract, TopologicalOrderRespectsEveryEdgeInFixture)
{
    const auto order = fixture.graph->query().topologicalOrder();
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->size(), 7u);

    auto position = [&order](NodeId id) -> std::ptrdiff_t {
        const auto it = std::find(order->begin(), order->end(), id);
        return it - order->begin();
    };

    for (EdgeId eid : fixture.edges)
    {
        const IEdge *e = fixture.graph->edge(eid);
        ASSERT_NE(e, nullptr);
        EXPECT_LT(position(e->from()), position(e->to()));
    }
}

TEST_P(QueryContract, TopologicalOrderAbsentOnCycle)
{
    addTestEdge(*fixture.graph,
                fixture.nodes[SevenNodeFixture::G],
                fixture.nodes[SevenNodeFixture::A]);
    EXPECT_FALSE(fixture.graph->query().topologicalOrder().has_value());
}

INSTANTIATE_TEST_SUITE_P(contract_query,
                         QueryContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::core::graph::contract
