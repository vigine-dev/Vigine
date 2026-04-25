#pragma once

#include <cstdint>

#include <vigine/core/graph/kind.h>  // INV-11 EXEMPTION: componentkind.h maps wrapper constants onto graph substrate ranges

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine
{
/**
 * @brief Non-template classifier for legacy components stored in
 *        @ref ComponentManager.
 *
 * @ref ComponentKind replaces the per-type-template path in the legacy
 * @ref ComponentManager so the public @ref IComponentManager surface
 * carries no template parameters (INV-1). Each value names the
 * concrete component family the manager stores; concrete components
 * report their kind through the virtual @ref IComponent::kind contract.
 *
 * Reserved ranges:
 *   - @c [0..127]   -- engine-bundled component kinds.
 *   - @c [128..255] -- user-space component kinds.
 *
 * The @c Unknown sentinel is the default-constructed value and exists so
 * that uninitialised handles do not accidentally compare equal to any
 * registered kind. Concrete subclasses that have not yet been migrated
 * onto the new contract may report @c Unknown until the migration leaf
 * lands; manager calls keyed on @c Unknown report
 * @ref Result::Code::Error.
 */
enum class ComponentKind : std::uint32_t
{
    Unknown = 0,
};

} // namespace vigine

namespace vigine::ecs
{
/**
 * @brief Node kind constants owned by the ECS wrapper.
 *
 * Carved out of the reserved range `[32..47]` in the graph substrate. Every
 * ECS-specific node carries one of these tags so the core graph stays free
 * of engine-specific concepts (see @ref vigine::core::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::core::graph::NodeKind Entity = 32;    // INV-11 EXEMPTION: kind constant mapping
inline constexpr vigine::core::graph::NodeKind Component = 33; // INV-11 EXEMPTION: kind constant mapping
} // namespace kind

/**
 * @brief Edge kind constants owned by the ECS wrapper.
 *
 * Carved out of the reserved range `[32..47]` in the graph substrate.
 */
namespace edge_kind
{
inline constexpr vigine::core::graph::EdgeKind Attached = 32;  // INV-11 EXEMPTION: kind constant mapping
} // namespace edge_kind

} // namespace vigine::ecs
