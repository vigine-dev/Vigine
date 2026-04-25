#pragma once

#include <cstdint>

namespace vigine::core::graph
{
/**
 * @brief Control flow directive returned by @ref IGraphVisitor.
 *
 * Closed enumeration. Returned from @ref IGraphVisitor::onNode and
 * @ref IGraphVisitor::onEdge to steer the traversal driver inside
 * @ref IGraph::traverse.
 */
enum class VisitResult : std::uint8_t
{
    Continue = 1, ///< Continue traversal normally.
    Skip     = 2, ///< Do not descend into the current subtree, keep walking.
    Stop     = 3, ///< Stop the traversal completely (early exit).
};

} // namespace vigine::core::graph
