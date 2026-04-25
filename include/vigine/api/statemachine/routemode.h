#pragma once

#include <cstdint>

namespace vigine::statemachine
{
/**
 * @brief Closed enumeration of the routing strategies an
 *        @ref IStateMachine uses when it resolves where an event goes
 *        inside a hierarchical state machine.
 *
 * The three values are the minimal set user decision UD-3 pinned for
 * hierarchical state machines:
 *
 *   - @c Direct    — deliver the event only to the current active
 *                    state. The traditional flat-FSM behaviour.
 *   - @c Bubble    — when the current (possibly child) state does not
 *                    handle the event, walk the parent chain via the
 *                    ChildOf edge of the internal topology until a
 *                    handler returns @c Handled or the root is
 *                    reached. Classical UML-statechart semantics.
 *   - @c Broadcast — deliver the event to every registered state
 *                    regardless of the hierarchy. Reserved for
 *                    cross-cutting diagnostic or shutdown signals.
 *
 * The enum is deliberately closed: adding a new mode is an API break
 * that requires an architect review per INV-1. The underlying type is
 * fixed at @c std::uint8_t so the enum is cheap to pass by value and
 * so its layout is portable across the messaging bus that eventually
 * consumes these values.
 *
 * @note The state machine wrapper ships its own @ref RouteMode rather
 *       than reusing the messaging core's @c vigine::messaging::RouteMode
 *       because the state-machine surface only needs the subset of
 *       modes that apply to hierarchical routing; letting every
 *       messaging mode leak through would widen the wrapper's
 *       contract beyond what UD-3 sanctions.
 */
enum class RouteMode : std::uint8_t
{
    Direct    = 1,
    Bubble    = 2,
    Broadcast = 3,
};

} // namespace vigine::statemachine
