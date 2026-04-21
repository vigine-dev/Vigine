#pragma once

#include <cstdint>

namespace vigine::graph
{
/**
 * @brief Strategy passed to @ref IGraph::traverse.
 *
 * Closed enumeration. Extending this set is a breaking API change because
 * every @ref IGraphVisitor implementation and every downstream wrapper
 * assumes the surface is exhaustive; adding a value requires coordinated
 * review of the engine.
 */
enum class TraverseMode : std::uint8_t
{
    DepthFirst         = 1, ///< DFS, pre-order (enter before children).
    BreadthFirst       = 2, ///< BFS, level by level.
    Topological        = 3, ///< DAG only; traversal reports an error on cycle.
    ReverseTopological = 4, ///< DAG only; reverse of Topological.
    Custom             = 5, ///< Visitor supplies the next node via
                            ///< @ref IGraphVisitor::nextForCustom.
};

} // namespace vigine::graph
