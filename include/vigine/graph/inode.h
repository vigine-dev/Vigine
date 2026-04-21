#pragma once

#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"

namespace vigine::graph
{
/**
 * @brief Pure-virtual vertex of an @ref IGraph.
 *
 * An @ref INode is the polymorphic hook every client plugs custom data
 * into. The graph owns the node after @ref IGraph::addNode succeeds and
 * never moves it thereafter, so implementations are pinned in memory for
 * the lifetime of their slot.
 *
 * Copy and move operations are deleted to keep the pinning invariant
 * explicit at the type level. Construct a concrete node on the heap and
 * transfer ownership to @ref IGraph::addNode via `std::unique_ptr`.
 *
 * Adjacency queries live on @ref IGraphQuery rather than on the node
 * itself so that the substrate stays minimal and implementations are free
 * to store topology in whichever layout they prefer.
 */
class INode
{
  public:
    virtual ~INode() = default;

    /**
     * @brief Returns the identifier assigned by the owning graph.
     *
     * Before @ref IGraph::addNode returns, the identifier is a
     * default-constructed (invalid) @ref NodeId. After, it reports the
     * generational slot assigned by the graph.
     */
    [[nodiscard]] virtual NodeId id() const noexcept = 0;

    /**
     * @brief Returns the kind tag selected by the concrete node.
     *
     * @see NodeKind
     */
    [[nodiscard]] virtual NodeKind kind() const noexcept = 0;

  protected:
    INode() = default;

  public:
    INode(const INode &)            = delete;
    INode &operator=(const INode &) = delete;
    INode(INode &&)                 = delete;
    INode &operator=(INode &&)      = delete;
};

} // namespace vigine::graph
