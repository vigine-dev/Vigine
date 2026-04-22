#pragma once

#include "vigine/graph/edgeid.h"
#include "vigine/graph/factory.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/iedgedata.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace vigine::graph::contract
{
/**
 * @brief Factory returning a new concrete IGraph behind the public API.
 *
 * The contract suite treats any factory that satisfies this signature as
 * a candidate for the full scenario list. The default factory wired up by
 * the suite in this leaf is the engine's own createGraph(); a second
 * concrete IGraph implementation plugs into the same suite by registering
 * another factory with INSTANTIATE_TEST_SUITE_P.
 */
using GraphFactory = std::function<std::unique_ptr<IGraph>()>;

/**
 * @brief Reserved edge kind for ChildOf relations used by the HSM-style
 *        scenarios.
 *
 * The value 49 is picked for the contract suite specifically; it sits in
 * the wrapper range (the core only defines kind::Generic = 1 and
 * edge_kind::Generic = 1), and the contract suite uses it to verify that
 * the graph does not reject kinds it does not know about.
 */
inline constexpr EdgeKind ContractChildOfKind = 49;

/**
 * @brief Reserved edge kind used by the FanOut and Broadcast style scenarios.
 */
inline constexpr EdgeKind ContractAttachedKind = 50;

/**
 * @brief Reserved node kind used by HSM-style scenarios.
 */
inline constexpr NodeKind ContractStateKind = 42;

/**
 * @brief Reserved node kind used by the FanOut and Broadcast style scenarios.
 */
inline constexpr NodeKind ContractSubscriberKind = 43;

/**
 * @brief Minimal IEdgeData payload used by the payload round-trip checks.
 *
 * Carries a stable dataTypeId and an integer tag so tests can assert the
 * payload survives addEdge / edge() round-trip unchanged.
 */
class TestEdgeData final : public IEdgeData
{
  public:
    explicit TestEdgeData(std::uint32_t typeId, int tag) noexcept
        : _typeId(typeId)
        , _tag(tag)
    {
    }

    [[nodiscard]] std::uint32_t dataTypeId() const noexcept override { return _typeId; }

    [[nodiscard]] int tag() const noexcept { return _tag; }

  private:
    std::uint32_t _typeId{0};
    int           _tag{0};
};

/**
 * @brief Minimal concrete INode used exclusively by the contract suite.
 *
 * The suite deliberately does not include the engine's own internal
 * NodeImpl from src/graph/defaultgraph.h; pulling an internal header into
 * the contract tests would couple the suite to one specific concrete
 * IGraph. Instead the suite ships its own INode subclass and exercises
 * the graph through addNode(std::unique_ptr<INode>) alone.
 */
class TestNode final : public INode
{
  public:
    explicit TestNode(NodeKind kind) noexcept : _kind(kind) {}

    [[nodiscard]] NodeId   id() const noexcept override { return _id; }
    [[nodiscard]] NodeKind kind() const noexcept override { return _kind; }

    /**
     * @brief Called by the owning test after addNode returns so that a
     *        later call to INode::id reports the graph-assigned value.
     *
     * The contract suite does not assume the graph's IdStamp hook exists,
     * so tests stamp the id explicitly after addNode.
     */
    void stampId(NodeId id) noexcept { _id = id; }

  private:
    NodeId   _id{};
    NodeKind _kind{};
};

/**
 * @brief Minimal concrete IEdge used exclusively by the contract suite.
 */
class TestEdge final : public IEdge
{
  public:
    TestEdge(NodeId from, NodeId to, EdgeKind kind, std::unique_ptr<IEdgeData> data) noexcept
        : _from(from)
        , _to(to)
        , _kind(kind)
        , _data(std::move(data))
    {
    }

    [[nodiscard]] EdgeId           id() const noexcept override { return _id; }
    [[nodiscard]] EdgeKind         kind() const noexcept override { return _kind; }
    [[nodiscard]] NodeId           from() const noexcept override { return _from; }
    [[nodiscard]] NodeId           to() const noexcept override { return _to; }
    [[nodiscard]] const IEdgeData *data() const noexcept override { return _data.get(); }

    void stampId(EdgeId id) noexcept { _id = id; }

  private:
    EdgeId                     _id{};
    NodeId                     _from{};
    NodeId                     _to{};
    EdgeKind                   _kind{};
    std::unique_ptr<IEdgeData> _data{};
};

/**
 * @brief Adds a node of the requested kind through the public IGraph API
 *        and back-stamps the test node so INode::id later reports the
 *        assigned value.
 *
 * Returns the graph-assigned NodeId. Returns NodeId{} only if the graph
 * rejects the insertion (which no current concrete IGraph does for a
 * non-null unique_ptr).
 */
inline NodeId addTestNode(IGraph &graph, NodeKind kind = kind::Generic)
{
    auto       rawNode = std::make_unique<TestNode>(kind);
    TestNode  *raw     = rawNode.get();
    const NodeId id    = graph.addNode(std::move(rawNode));
    raw->stampId(id);
    return id;
}

/**
 * @brief Adds an edge through the public IGraph API and back-stamps the
 *        test edge.
 */
inline EdgeId addTestEdge(IGraph                   &graph,
                          NodeId                    from,
                          NodeId                    to,
                          EdgeKind                  kind = edge_kind::Generic,
                          std::unique_ptr<IEdgeData> data = nullptr)
{
    auto        rawEdge = std::make_unique<TestEdge>(from, to, kind, std::move(data));
    TestEdge   *raw     = rawEdge.get();
    const EdgeId id     = graph.addEdge(std::move(rawEdge));
    raw->stampId(id);
    return id;
}

/**
 * @brief Seven-node, ten-edge fixture used by the traversal, query, and
 *        scenario suites.
 *
 * Topology:
 *
 *      A ---> B ---> D
 *      |      |      |
 *      v      v      v
 *      C ---> E ---> F
 *             |
 *             v
 *             G
 *
 * Plus a cross edge C -> D and a back-link E -> A omitted by default
 * (tests that want a cycle add it explicitly).
 */
struct SevenNodeFixture
{
    std::unique_ptr<IGraph> graph;
    std::array<NodeId, 7>   nodes{};
    std::array<EdgeId, 10>  edges{};

    enum : std::size_t
    {
        A = 0,
        B = 1,
        C = 2,
        D = 3,
        E = 4,
        F = 5,
        G = 6,
    };

    /**
     * @brief Populates @p graph with the seven-node ten-edge shape and
     *        records the assigned ids.
     *
     * The ten edges, in insertion order, are:
     *   0: A -> B
     *   1: A -> C
     *   2: B -> D
     *   3: B -> E
     *   4: C -> E
     *   5: C -> D
     *   6: D -> F
     *   7: E -> F
     *   8: E -> G
     *   9: F -> G
     */
    void build(const GraphFactory &factory)
    {
        graph = factory();
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            nodes[i] = addTestNode(*graph);
        }
        const auto a = nodes[A];
        const auto b = nodes[B];
        const auto c = nodes[C];
        const auto d = nodes[D];
        const auto e = nodes[E];
        const auto f = nodes[F];
        const auto g = nodes[G];
        edges[0]     = addTestEdge(*graph, a, b);
        edges[1]     = addTestEdge(*graph, a, c);
        edges[2]     = addTestEdge(*graph, b, d);
        edges[3]     = addTestEdge(*graph, b, e);
        edges[4]     = addTestEdge(*graph, c, e);
        edges[5]     = addTestEdge(*graph, c, d);
        edges[6]     = addTestEdge(*graph, d, f);
        edges[7]     = addTestEdge(*graph, e, f);
        edges[8]     = addTestEdge(*graph, e, g);
        edges[9]     = addTestEdge(*graph, f, g);
    }
};

/**
 * @brief Base fixture used by every parametrised contract suite.
 *
 * Takes a GraphFactory through TEST_P's parameter and constructs one
 * graph per test. Test bodies reference only the IGraph public surface
 * and the helpers above, never a concrete graph type.
 */
class ContractFixture : public ::testing::TestWithParam<GraphFactory>
{
  protected:
    std::unique_ptr<IGraph> makeGraph() const { return GetParam()(); }
};

/**
 * @brief Parametrised fixture that preloads the seven-node ten-edge shape.
 */
class SevenNodeParamFixture : public ::testing::TestWithParam<GraphFactory>
{
  protected:
    void SetUp() override { fixture.build(GetParam()); }

    SevenNodeFixture fixture;
};

/**
 * @brief Returns the default factory used by the contract suite in this
 *        leaf: the engine's own createGraph.
 *
 * A second concrete IGraph wires into the suite by registering another
 * GraphFactory with INSTANTIATE_TEST_SUITE_P.
 */
inline GraphFactory defaultGraphFactory()
{
    return [] { return createGraph(); };
}

/**
 * @brief Name generator used by INSTANTIATE_TEST_SUITE_P so that the
 *        parametrised test names read e.g. `DefaultGraph/LifecycleContract.X`
 *        instead of printing the raw GraphFactory bytes.
 *
 * The suite in this leaf has a single parameter (the engine's own
 * createGraph); when future leaves register additional implementations
 * they pass their own label to INSTANTIATE_TEST_SUITE_P as the first
 * argument, and this generator just reuses the index-based fallback
 * supplied by GoogleTest.
 */
struct GraphFactoryNamer
{
    template <class ParamInfo>
    std::string operator()(const ParamInfo &info) const
    {
        return "case" + std::to_string(info.index);
    }
};

} // namespace vigine::graph::contract
