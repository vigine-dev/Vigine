#pragma once

#include <cstdint>

namespace vigine::taskflow
{
/**
 * @brief Closed enumeration of the outcome codes a task reports back
 *        to its task flow when it finishes executing.
 *
 * User decision UD-3 fixed the minimal set of outcome codes the task
 * flow wrapper needs to route synchronously. The four values are the
 * smallest set that covers both the back-compat path (@c Success vs.
 * @c Error matching the legacy @c Result::Code shape) and the two UD-3
 * extensions callers rely on when they wire up non-trivial flows:
 *
 *   - @c Success  — the task completed successfully. The task flow
 *                   routes to whichever next task is registered
 *                   against @c Success from this source.
 *   - @c Error    — the task failed. The task flow routes to whichever
 *                   next task is registered against @c Error from this
 *                   source, or halts the flow when no route is
 *                   registered.
 *   - @c Deferred — the task declined to route synchronously and will
 *                   signal the next transition asynchronously later.
 *                   The wrapper in this leaf only stores the outcome
 *                   code; the signal-driven routing that consumes
 *                   @c Deferred lands with the facade wiring in a
 *                   later leaf.
 *   - @c Skip     — the task chose to skip routing for this outcome.
 *                   The task flow neither advances nor reports an
 *                   error; the caller typically uses @c Skip to short-
 *                   circuit a branch without aborting the flow.
 *
 * The enum is deliberately closed: adding a new code is an API break
 * that requires an architect review per INV-1. The underlying type is
 * fixed at @c std::uint8_t so the enum is cheap to pass by value and
 * so its layout is portable across the messaging bus that eventually
 * consumes these values.
 *
 * @note The task flow wrapper ships its own @ref ResultCode rather
 *       than reusing the engine's @c vigine::Result::Code because the
 *       flow surface only needs the four outcomes above; widening the
 *       surface to the general @c Result::Code set would pull in
 *       codes (e.g. @c DuplicatePayloadId, @c SubscriptionExpired)
 *       that have no meaning inside a task transition and would
 *       obscure the routing contract callers rely on.
 */
enum class ResultCode : std::uint8_t
{
    Success  = 1,
    Error    = 2,
    Deferred = 3,
    Skip     = 4,
};

} // namespace vigine::taskflow
