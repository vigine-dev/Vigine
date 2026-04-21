#pragma once

#include <vigine/graph/kind.h>

// wrapper-kind-waiver: engine-concept kind constants in a wrapper subspace.

namespace vigine::fsm
{
/**
 * @brief Node kind constants owned by the state machine wrapper.
 *
 * Carved out of the reserved range `[48..63]` in the graph substrate. Every
 * state-machine node carries one of these tags so the core graph stays free
 * of engine-specific concepts (see @ref vigine::graph::NodeKind).
 */
namespace kind
{
inline constexpr vigine::graph::NodeKind State = 48;
} // namespace kind

/**
 * @brief Edge kind constants owned by the state machine wrapper.
 *
 * Carved out of the reserved range `[48..63]` in the graph substrate.
 * `ChildOf` models the parent/child relationship inside a hierarchical state
 * machine; `Transition` models a labelled transition between sibling states.
 */
namespace edge_kind
{
inline constexpr vigine::graph::EdgeKind Transition = 48;
inline constexpr vigine::graph::EdgeKind ChildOf = 49;
} // namespace edge_kind

} // namespace vigine::fsm
