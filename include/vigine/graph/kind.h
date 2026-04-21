#pragma once

#include <cstdint>

namespace vigine::graph
{
/**
 * @brief Byte-wide tag classifying a node.
 *
 * Open-ended alias rather than a closed enum so downstream wrappers can
 * register their own values without forcing a breaking change to the core
 * graph. Reserved value ranges are documented per wrapper; the core itself
 * only defines @ref kind::Generic.
 */
using NodeKind = std::uint8_t;

/**
 * @brief Byte-wide tag classifying an edge.
 *
 * Mirrors @ref NodeKind. Wrappers (messaging, ECS, state machine, task
 * flow) supply their own named constants in their public headers; the core
 * only defines @ref edge_kind::Generic.
 */
using EdgeKind = std::uint8_t;

/**
 * @brief Node kind constants owned by the graph core.
 *
 * Wrappers MUST NOT put their own constants in this namespace; they supply
 * their own (for example `vigine::messaging::kind`). Keeping the core
 * namespace free of engine-specific concepts is a structural invariant of
 * the graph substrate.
 */
namespace kind
{
inline constexpr NodeKind Generic = 1;
} // namespace kind

/**
 * @brief Edge kind constants owned by the graph core.
 */
namespace edge_kind
{
inline constexpr EdgeKind Generic = 1;
} // namespace edge_kind

} // namespace vigine::graph
