#pragma once

#include <cstdint>

namespace vigine::taskflow
{
/**
 * @brief Closed enumeration of the routing strategies an
 *        @ref ITaskFlow uses when it resolves which next task runs
 *        after a completed task reports a given @ref ResultCode.
 *
 * The three values are the minimal set user decision UD-3 pinned for
 * task flow transitions:
 *
 *   - @c FirstMatch — deliver to the first next task registered for
 *                     this @c (source, resultCode) pair. Later
 *                     registrations with the same pair are ignored
 *                     during routing. The back-compatible default
 *                     that matches the legacy task flow behaviour.
 *   - @c FanOut     — deliver to every next task registered for this
 *                     @c (source, resultCode) pair. Callers opt in
 *                     per registration; each matching target runs
 *                     independently.
 *   - @c Chain      — deliver sequentially through every next task
 *                     registered for this @c (source, resultCode)
 *                     pair, one after another, forming a pipeline.
 *                     The next task in the chain starts when the
 *                     current one finishes with @c Success.
 *
 * The enum is deliberately closed: adding a new mode is an API break
 * that requires an architect review per INV-1. The underlying type is
 * fixed at @c std::uint8_t so the enum is cheap to pass by value and
 * so its layout is portable across the messaging bus that eventually
 * consumes these values.
 *
 * @note The task flow wrapper ships its own @ref RouteMode rather
 *       than reusing the messaging core's @c vigine::messaging::RouteMode
 *       because the task-flow surface only needs the three modes that
 *       apply to task transitions; letting every messaging mode
 *       (@c Bubble, @c Broadcast) leak through would widen the
 *       wrapper's contract beyond what UD-3 sanctions.
 */
enum class RouteMode : std::uint8_t
{
    FirstMatch = 1,
    FanOut     = 2,
    Chain      = 3,
};

} // namespace vigine::taskflow
