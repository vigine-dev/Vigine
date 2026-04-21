#include "fixtures/graph_fixture_7n10e.h"

#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/nodeid.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <unordered_set>
#include <vector>

// =============================================================================
// Scenario j — generational identifier safety.
//
// Exercises the generational-id contract by running N = 32 rounds of
// (create + remove + recreate). The suite asserts:
//
//   1. No stale NodeId or EdgeId from a removed slot ever resolves to a
//      live object through IGraph::node or IGraph::edge (no aliasing).
//   2. The count surfaces (nodeCount / edgeCount) match the number of
//      currently-live slots after each round.
//   3. Recreation after removal issues fresh identifiers distinct from
//      every previously-issued id.
//
// Writing the test against the IGraph surface alone means any concrete
// graph with generational ids (as required by plan_01 / plan_02) passes.
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

struct EdgeIdHashPolicy
{
    std::size_t operator()(EdgeId id) const noexcept
    {
        return (static_cast<std::size_t>(id.index) << 32)
               ^ static_cast<std::size_t>(id.generation);
    }
};

} // namespace

using GenerationalIdContract = ContractFixture;

TEST_P(GenerationalIdContract, ThirtyTwoNodeRoundsRetainNoStaleAliases)
{
    constexpr std::size_t kRounds = 32;
    auto                  graph   = makeGraph();
    std::vector<NodeId>   every;
    every.reserve(kRounds * 3);

    for (std::size_t round = 0; round < kRounds; ++round)
    {
        const NodeId first = addTestNode(*graph);
        every.push_back(first);
        ASSERT_TRUE(first.valid());
        ASSERT_NE(graph->node(first), nullptr);

        ASSERT_TRUE(graph->removeNode(first).isSuccess());
        EXPECT_EQ(graph->node(first), nullptr);

        const NodeId reborn = addTestNode(*graph);
        every.push_back(reborn);
        ASSERT_TRUE(reborn.valid());
        EXPECT_NE(graph->node(reborn), nullptr);
        EXPECT_EQ(graph->node(first), nullptr);
        EXPECT_NE(first, reborn);

        // And once more so index reuse is exercised twice per round.
        ASSERT_TRUE(graph->removeNode(reborn).isSuccess());
        EXPECT_EQ(graph->node(reborn), nullptr);

        const NodeId thrice = addTestNode(*graph);
        every.push_back(thrice);
        EXPECT_NE(graph->node(thrice), nullptr);
        EXPECT_EQ(graph->node(first), nullptr);
        EXPECT_EQ(graph->node(reborn), nullptr);
    }

    // Live count is whatever remains from the final create in each round.
    EXPECT_EQ(graph->nodeCount(), kRounds);

    // No two ids returned across every round collide — each id is unique.
    std::unordered_set<NodeId, NodeIdHashPolicy> uniq(every.begin(), every.end());
    EXPECT_EQ(uniq.size(), every.size());
}

TEST_P(GenerationalIdContract, ThirtyTwoEdgeRoundsRetainNoStaleAliases)
{
    constexpr std::size_t kRounds = 32;
    auto                  graph   = makeGraph();
    const NodeId          a       = addTestNode(*graph);
    const NodeId          b       = addTestNode(*graph);

    std::vector<EdgeId> every;
    every.reserve(kRounds * 3);

    for (std::size_t round = 0; round < kRounds; ++round)
    {
        const EdgeId first = addTestEdge(*graph, a, b);
        every.push_back(first);
        ASSERT_TRUE(first.valid());
        ASSERT_NE(graph->edge(first), nullptr);

        ASSERT_TRUE(graph->removeEdge(first).isSuccess());
        EXPECT_EQ(graph->edge(first), nullptr);

        const EdgeId reborn = addTestEdge(*graph, a, b);
        every.push_back(reborn);
        EXPECT_NE(graph->edge(reborn), nullptr);
        EXPECT_EQ(graph->edge(first), nullptr);
        EXPECT_NE(first, reborn);

        ASSERT_TRUE(graph->removeEdge(reborn).isSuccess());
        EXPECT_EQ(graph->edge(reborn), nullptr);

        const EdgeId thrice = addTestEdge(*graph, a, b);
        every.push_back(thrice);
        EXPECT_NE(graph->edge(thrice), nullptr);
        EXPECT_EQ(graph->edge(first), nullptr);
        EXPECT_EQ(graph->edge(reborn), nullptr);
    }

    EXPECT_EQ(graph->edgeCount(), kRounds);
    std::unordered_set<EdgeId, EdgeIdHashPolicy> uniq(every.begin(), every.end());
    EXPECT_EQ(uniq.size(), every.size());
}

TEST_P(GenerationalIdContract, StaleIdDoesNotSurviveCascadeRemoval)
{
    auto         graph = makeGraph();
    const NodeId a     = addTestNode(*graph);
    const NodeId b     = addTestNode(*graph);
    const EdgeId e     = addTestEdge(*graph, a, b);

    ASSERT_TRUE(graph->removeNode(a).isSuccess());
    EXPECT_EQ(graph->edge(e), nullptr);

    // Recreating a new edge between other live nodes must not revive @p e.
    const NodeId c = addTestNode(*graph);
    const NodeId d = addTestNode(*graph);
    const EdgeId f = addTestEdge(*graph, c, d);
    EXPECT_NE(f, e);
    EXPECT_EQ(graph->edge(e), nullptr);
}

INSTANTIATE_TEST_SUITE_P(contract_generational_id,
                         GenerationalIdContract,
                         ::testing::Values(defaultGraphFactory()),
                         GraphFactoryNamer{});

} // namespace vigine::graph::contract
