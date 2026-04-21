#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/igraph.h"
#include "vigine/graph/igraphquery.h"
#include "vigine/graph/igraphvisitor.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"
#include "vigine/graph/traverse_mode.h"
#include "vigine/result.h"

namespace vigine::graph
{
/**
 * @brief Concrete stateful base for every in-memory @ref IGraph.
 *
 * @ref AbstractGraph carries the shared storage, the generational id
 * machinery, the reader-writer mutex, and the version counter that every
 * concrete graph implementation and every wrapper built on the graph
 * substrate relies on. It is the common foundation of the five-layer
 * recipe used by the engine's Level-1 wrappers (messaging, ECS, state
 * machine, task flow, service container), all of which inherit
 * @ref AbstractGraph as @c protected to reuse the substrate without
 * re-exposing it on the wrapper's public surface.
 *
 * The class carries state, so it follows the project's @c Abstract naming
 * convention rather than the @c I pure-virtual prefix. It is abstract only
 * in the logical sense — users do not instantiate it directly because the
 * factory in @ref factory.h returns a @c final subclass (@ref DefaultGraph)
 * that merely marks the inheritance chain closed.
 *
 * Thread-safety: every mutating entry point takes an exclusive lock on the
 * internal @c std::shared_mutex; every read-only entry point takes a shared
 * lock. Traversal snapshots the structural information it needs under a
 * shared lock and then runs the visitor callbacks lock-free, so a visitor
 * that re-enters the graph does not deadlock and so concurrent readers do
 * not block each other.
 */
class AbstractGraph : public IGraph
{
  public:
    ~AbstractGraph() override;

    // ------ IGraph: node lifecycle ------

    [[nodiscard]] NodeId addNode(std::unique_ptr<INode> node) override;
    Result               removeNode(NodeId id) override;
    [[nodiscard]] const INode *node(NodeId id) const noexcept override;

    // ------ IGraph: edge lifecycle ------

    [[nodiscard]] EdgeId addEdge(std::unique_ptr<IEdge> edge) override;
    Result               removeEdge(EdgeId id) override;
    [[nodiscard]] const IEdge *edge(EdgeId id) const noexcept override;

    // ------ IGraph: traversal ------

    Result traverse(NodeId startNode, TraverseMode mode, IGraphVisitor &visitor) override;

    // ------ IGraph: query ------

    [[nodiscard]] const IGraphQuery &query() const noexcept override;

    // ------ IGraph: observability ------

    [[nodiscard]] std::size_t nodeCount() const noexcept override;
    [[nodiscard]] std::size_t edgeCount() const noexcept override;

    // ------ IGraph: tooling ------

    Result exportGraphViz(std::string &out) const override;

    /**
     * @brief Optional mixin nodes and edges inherit when they want their
     *        own generational id back from the graph at insertion time.
     *
     * Nodes and edges that do not need to cache their own id just ignore
     * the mixin and return @c NodeId{} / @c EdgeId{} from their
     * @ref INode::id / @ref IEdge::id methods. The graph internally
     * tracks the canonical mapping either way; this hook exists only so
     * that @ref INode::id / @ref IEdge::id can report the assigned id
     * without round-tripping through the graph.
     */
    class IdStamp
    {
      public:
        virtual ~IdStamp() = default;
        /**
         * @brief Called once by the graph immediately after the node or
         *        edge has been placed into its storage slot. Exactly one
         *        of the two arguments carries a valid id; the other is
         *        a default-constructed sentinel.
         */
        virtual void onGraphIdAssigned(NodeId nodeId, EdgeId edgeId) noexcept = 0;

      protected:
        IdStamp() = default;
    };

  protected:
    AbstractGraph();

  private:
    struct NodeSlot
    {
        std::unique_ptr<INode> node;
        std::uint32_t          generation{0};
        std::vector<EdgeId>    outEdges;
        std::vector<EdgeId>    inEdges;
    };

    struct EdgeSlot
    {
        std::unique_ptr<IEdge> edge;
        std::uint32_t          generation{0};
    };

    class QueryImpl final : public IGraphQuery
    {
      public:
        explicit QueryImpl(const AbstractGraph &graph);

        [[nodiscard]] bool hasNode(NodeId id) const noexcept override;
        [[nodiscard]] bool hasEdge(EdgeId id) const noexcept override;
        [[nodiscard]] std::vector<EdgeId> outEdges(NodeId id) const override;
        [[nodiscard]] std::vector<EdgeId> inEdges(NodeId id) const override;
        [[nodiscard]] std::vector<EdgeId> outEdgesOfKind(NodeId id, EdgeKind kind) const override;
        [[nodiscard]] std::vector<EdgeId> inEdgesOfKind(NodeId id, EdgeKind kind) const override;
        [[nodiscard]] std::optional<std::vector<NodeId>>
            shortestPath(NodeId from, NodeId to) const override;
        [[nodiscard]] std::vector<std::vector<NodeId>> connectedComponents() const override;
        [[nodiscard]] bool                             hasCycle() const override;
        [[nodiscard]] std::optional<std::vector<NodeId>>
            topologicalOrder() const override;

      private:
        const AbstractGraph &_graph;
    };

    // Snapshot shape used by the traversal driver and by the structural
    // query methods. Copying the relevant adjacency into plain vectors lets
    // the caller drive its algorithm lock-free once the snapshot is done.
    struct Snapshot
    {
        struct Endpoints
        {
            NodeId from;
            NodeId to;
        };
        std::vector<NodeId>                                   nodes;
        std::unordered_map<std::uint64_t, std::vector<EdgeId>> outByKey;
        std::unordered_map<std::uint64_t, std::vector<EdgeId>> inByKey;
        std::unordered_map<std::uint64_t, Endpoints>           edgeEndpoints;

        static std::uint64_t nodeKey(NodeId id) noexcept
        {
            return (static_cast<std::uint64_t>(id.index) << 32)
                   | static_cast<std::uint64_t>(id.generation);
        }
        static std::uint64_t edgeKey(EdgeId id) noexcept
        {
            return (static_cast<std::uint64_t>(id.index) << 32)
                   | static_cast<std::uint64_t>(id.generation);
        }
    };

    Snapshot buildSnapshot() const;

    // Private helpers used by traversal / queries.
    std::vector<NodeId> snapshotLiveNodes() const;
    std::vector<EdgeId> snapshotOutEdges(NodeId id) const;
    bool                eraseEdgeLocked(EdgeId id);
    static void         stampOnInsert(INode &node, NodeId id) noexcept;
    static void         stampOnInsert(IEdge &edge, EdgeId id) noexcept;

    mutable std::shared_mutex                         _mutex;
    std::unordered_map<std::uint32_t, NodeSlot>       _nodes;
    std::unordered_map<std::uint32_t, EdgeSlot>       _edges;
    std::uint32_t                                     _nextNodeIndex{1};
    std::uint32_t                                     _nextEdgeIndex{1};
    std::atomic<std::uint64_t>                        _version{0};
    QueryImpl                                         _query;

    friend class QueryImpl;
};

} // namespace vigine::graph
