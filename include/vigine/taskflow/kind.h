#pragma once

#include <vigine/graph/kind.h>

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine::taskflow
{
/**
 * @brief Node kind constants owned by the task flow wrapper.
 *
 * Carved out of the reserved range `[64..79]` in the graph substrate. Every
 * task-flow node carries one of these tags so the core graph stays free of
 * engine-specific concepts (see @ref vigine::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::graph::NodeKind Task = 64;
} // namespace kind

/**
 * @brief Edge kind constants owned by the task flow wrapper.
 *
 * Carved out of the reserved range `[64..79]` in the graph substrate.
 */
namespace edge_kind
{
inline constexpr vigine::graph::EdgeKind Transition = 64;
} // namespace edge_kind

} // namespace vigine::taskflow
