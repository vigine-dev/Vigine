#pragma once

#include <vigine/graph/kind.h>  // INV-11 EXEMPTION: kind.h maps wrapper constants onto graph substrate ranges

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine::messaging
{
/**
 * @brief Node kind constants owned by the messaging wrapper.
 *
 * Carved out of the reserved range `[16..31]` in the graph substrate. Every
 * messaging-specific node carries one of these tags so the core graph stays
 * free of engine-specific concepts (see @ref vigine::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::graph::NodeKind Target = 16;       // INV-11 EXEMPTION: kind constant mapping
inline constexpr vigine::graph::NodeKind Subscription = 17; // INV-11 EXEMPTION: kind constant mapping
} // namespace kind

/**
 * @brief Edge kind constants owned by the messaging wrapper.
 *
 * Carved out of the reserved range `[16..31]` in the graph substrate.
 */
namespace edge_kind
{
inline constexpr vigine::graph::EdgeKind Subscription = 16; // INV-11 EXEMPTION: kind constant mapping
} // namespace edge_kind

} // namespace vigine::messaging
