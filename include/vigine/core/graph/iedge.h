#pragma once

#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/iedgedata.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"

namespace vigine::core::graph
{
/**
 * @brief Pure-virtual directed edge of an @ref IGraph.
 *
 * Each edge knows its own identifier, its endpoints, its kind tag, and
 * carries an optional polymorphic @ref IEdgeData payload. The graph owns
 * the edge after @ref IGraph::addEdge succeeds; ownership is transferred
 * via `std::unique_ptr`.
 *
 * Copy and move operations are deleted for the same pinning reason as
 * @ref INode.
 */
class IEdge
{
  public:
    virtual ~IEdge() = default;

    /**
     * @brief Returns the identifier assigned by the owning graph.
     */
    [[nodiscard]] virtual EdgeId id() const noexcept = 0;

    /**
     * @brief Returns the kind tag selected by the concrete edge.
     */
    [[nodiscard]] virtual EdgeKind kind() const noexcept = 0;

    /**
     * @brief Returns the source vertex of the directed edge.
     */
    [[nodiscard]] virtual NodeId from() const noexcept = 0;

    /**
     * @brief Returns the target vertex of the directed edge.
     */
    [[nodiscard]] virtual NodeId to() const noexcept = 0;

    /**
     * @brief Returns the optional polymorphic payload.
     *
     * Returns `nullptr` when the edge carries no payload. Callers branch on
     * @ref IEdgeData::dataTypeId to decide how to downcast.
     */
    [[nodiscard]] virtual const IEdgeData *data() const noexcept = 0;

  protected:
    IEdge() = default;

  public:
    IEdge(const IEdge &)            = delete;
    IEdge &operator=(const IEdge &) = delete;
    IEdge(IEdge &&)                 = delete;
    IEdge &operator=(IEdge &&)      = delete;
};

} // namespace vigine::core::graph
