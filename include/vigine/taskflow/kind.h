#pragma once

#include <vigine/core/graph/kind.h>  // INV-11 EXEMPTION: kind.h maps wrapper constants onto graph substrate ranges

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine::taskflow
{
/**
 * @brief Node kind constants owned by the task flow wrapper.
 *
 * Carved out of the reserved range `[64..79]` in the graph substrate. Every
 * task-flow node carries one of these tags so the core graph stays free of
 * engine-specific concepts (see @ref vigine::core::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::core::graph::NodeKind Task = 64;       // INV-11 EXEMPTION: kind constant mapping
} // namespace kind

/**
 * @brief Edge kind constants owned by the task flow wrapper.
 *
 * Carved out of the reserved range `[64..79]` in the graph substrate.
 */
namespace edge_kind
{
inline constexpr vigine::core::graph::EdgeKind Transition = 64; // INV-11 EXEMPTION: kind constant mapping
} // namespace edge_kind

} // namespace vigine::taskflow
