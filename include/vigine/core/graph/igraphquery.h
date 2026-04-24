#pragma once

#include <optional>
#include <vector>

#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"

namespace vigine::core::graph
{
/**
 * @brief Pure-virtual read-only query surface of an @ref IGraph.
 *
 * Every method is `const`. Implementations are free to cache indices and
 * materialise results lazily; callers treat the returned vectors as
 * snapshots valid at call time.
 *
 * Neighbourhood queries return edge identifiers rather than node
 * identifiers because a single neighbour can be connected by several
 * edges of different kinds; the edge view is therefore the canonical
 * primitive, and consumers navigate to the other endpoint through
 * @ref IGraph::edge.
 */
class IGraphQuery
{
  public:
    virtual ~IGraphQuery() = default;

    // ------ Node / edge existence ------

    /**
     * @brief Returns `true` when the identifier refers to a live node.
     */
    [[nodiscard]] virtual bool hasNode(NodeId id) const noexcept = 0;

    /**
     * @brief Returns `true` when the identifier refers to a live edge.
     */
    [[nodiscard]] virtual bool hasEdge(EdgeId id) const noexcept = 0;

    // ------ Directed neighbourhood ------

    /**
     * @brief Returns identifiers of edges leaving @p id.
     */
    [[nodiscard]] virtual std::vector<EdgeId> outEdges(NodeId id) const = 0;

    /**
     * @brief Returns identifiers of edges arriving at @p id.
     */
    [[nodiscard]] virtual std::vector<EdgeId> inEdges(NodeId id) const = 0;

    /**
     * @brief Returns edges leaving @p id whose kind equals @p kind.
     */
    [[nodiscard]] virtual std::vector<EdgeId> outEdgesOfKind(NodeId id, EdgeKind kind) const = 0;

    /**
     * @brief Returns edges arriving at @p id whose kind equals @p kind.
     */
    [[nodiscard]] virtual std::vector<EdgeId> inEdgesOfKind(NodeId id, EdgeKind kind) const = 0;

    // ------ Structural queries ------

    /**
     * @brief Returns the shortest unweighted path from @p from to @p to.
     *
     * Returns an empty optional when no path exists or when either
     * identifier is stale.
     */
    [[nodiscard]] virtual std::optional<std::vector<NodeId>>
        shortestPath(NodeId from, NodeId to) const = 0;

    /**
     * @brief Returns the list of connected components (treated as
     *        undirected).
     */
    [[nodiscard]] virtual std::vector<std::vector<NodeId>>
        connectedComponents() const = 0;

    /**
     * @brief Returns `true` when the graph contains at least one directed
     *        cycle.
     */
    [[nodiscard]] virtual bool hasCycle() const = 0;

    /**
     * @brief Returns a topological ordering when the graph is acyclic.
     *
     * Returns an empty optional when the graph has a cycle.
     */
    [[nodiscard]] virtual std::optional<std::vector<NodeId>>
        topologicalOrder() const = 0;

  protected:
    IGraphQuery() = default;

  public:
    IGraphQuery(const IGraphQuery &)            = delete;
    IGraphQuery &operator=(const IGraphQuery &) = delete;
    IGraphQuery(IGraphQuery &&)                 = delete;
    IGraphQuery &operator=(IGraphQuery &&)      = delete;
};

} // namespace vigine::core::graph
