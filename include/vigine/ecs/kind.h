#pragma once

#include <vigine/graph/kind.h>

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine::ecs
{
/**
 * @brief Node kind constants owned by the ECS wrapper.
 *
 * Carved out of the reserved range `[32..47]` in the graph substrate. Every
 * ECS-specific node carries one of these tags so the core graph stays free
 * of engine-specific concepts (see @ref vigine::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::graph::NodeKind Entity = 32;
inline constexpr vigine::graph::NodeKind Component = 33;
} // namespace kind

/**
 * @brief Edge kind constants owned by the ECS wrapper.
 *
 * Carved out of the reserved range `[32..47]` in the graph substrate.
 */
namespace edge_kind
{
inline constexpr vigine::graph::EdgeKind Attached = 32;
} // namespace edge_kind

} // namespace vigine::ecs
