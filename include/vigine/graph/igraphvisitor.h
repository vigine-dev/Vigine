#pragma once

#include "vigine/graph/iedge.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/nodeid.h"
#include "vigine/graph/visit_result.h"

namespace vigine::graph
{
/**
 * @brief Pure-virtual callback consumed by @ref IGraph::traverse.
 *
 * The traversal driver calls @ref onNode as it enters a vertex and
 * @ref onEdge as it crosses an outbound edge. The driver honours the
 * returned @ref VisitResult: `Continue` keeps walking, `Skip` prunes the
 * current subtree, `Stop` terminates the whole traversal.
 *
 * For @ref TraverseMode::Custom the driver additionally calls
 * @ref nextForCustom to let the visitor choose the next target vertex
 * manually; returning an invalid @ref NodeId ends the walk.
 */
class IGraphVisitor
{
  public:
    virtual ~IGraphVisitor() = default;

    /**
     * @brief Invoked as traversal enters a vertex (pre-order for DFS).
     */
    virtual VisitResult onNode(const INode &node) = 0;

    /**
     * @brief Invoked before traversal descends along an outbound edge.
     */
    virtual VisitResult onEdge(const IEdge &edge) = 0;

    /**
     * @brief Supplies the next vertex for @ref TraverseMode::Custom.
     *
     * Default returns an invalid @ref NodeId, which ends traversal. Visitors
     * that do not use custom mode simply inherit the default.
     */
    [[nodiscard]] virtual NodeId nextForCustom(const INode &current) { (void)current; return NodeId{}; }

  protected:
    IGraphVisitor() = default;

  public:
    IGraphVisitor(const IGraphVisitor &)            = delete;
    IGraphVisitor &operator=(const IGraphVisitor &) = delete;
    IGraphVisitor(IGraphVisitor &&)                 = delete;
    IGraphVisitor &operator=(IGraphVisitor &&)      = delete;
};

} // namespace vigine::graph
