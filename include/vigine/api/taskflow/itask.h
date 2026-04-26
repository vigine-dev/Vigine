#pragma once

/**
 * @file itask.h
 * @brief Pure-virtual contract for a single unit of work executed by
 *        the task flow.
 *
 * @ref ITask is the task-side half of the @ref vigine::engine::IEngineToken
 * contract documented in @c architecture.md § R-StateScope. The task
 * flow hands every concrete task a state-scoped @ref IEngineToken via
 * @ref setApi before invoking @ref run; tasks reach engine subsystems
 * exclusively through the bound token returned by @ref api.
 *
 * Lifecycle:
 *   - @ref setApi binds a non-owning @ref vigine::engine::IEngineToken
 *     pointer for the upcoming @ref run call. The pointer must remain
 *     live for the duration of the @ref run invocation; the engine
 *     guarantees this by issuing the token, calling @ref run
 *     synchronously inside the bound state's scope, and only then
 *     letting the token expire.
 *   - @ref api returns the bound token (or @c nullptr when no token
 *     has been bound yet). Callers branch on the return value before
 *     dereferencing.
 *   - @ref run is the canonical task-facing entry point. The task
 *     flow calls it once per scheduled tick. The return @ref Result
 *     drives the result-code-keyed transitions registered on the
 *     enclosing @ref vigine::taskflow::ITaskFlow.
 *
 * Self-destruct contract (architecture.md § R-StateScope, mechanism
 * step 5): a task whose bound state has been torn down out from under
 * it sees gated accessors on @ref api return
 * @ref vigine::engine::Result::Code::Expired. The task MUST cooperate
 * by returning from @ref run promptly with an error @ref Result rather
 * than racing the engine through additional gated calls. The engine
 * does not interrupt or kill running tasks.
 *
 * Invariants:
 *   - INV-10: @c I prefix for this pure-virtual interface (no state,
 *             no non-virtual method bodies).
 *   - INV-12: no data members on this pure interface; the stateful
 *             base @ref vigine::AbstractTask carries the bound
 *             @ref vigine::engine::IEngineToken pointer.
 */

#include "vigine/result.h"

namespace vigine::engine
{
class IEngineToken;
} // namespace vigine::engine

namespace vigine
{

/**
 * @brief Pure-virtual contract for a unit of work scheduled by the
 *        task flow.
 *
 * Concrete tasks derive from @ref AbstractTask, which implements
 * @ref setApi and @ref api as @c final and leaves @ref run pure
 * virtual. The task flow binds a state-scoped
 * @ref vigine::engine::IEngineToken via @ref setApi before invoking
 * @ref run.
 *
 * Thread-safety: the contract does not pin one. The default
 * implementation in @ref AbstractTask runs every method on the
 * thread that drives the task flow's tick (typically the controller
 * thread). Concrete tasks may dispatch work to other threads via
 * @ref vigine::engine::IEngineToken::threadManager but must complete
 * their own @ref run on the calling thread before returning.
 */
class ITask
{
  public:
    virtual ~ITask() = default;

    /**
     * @brief Binds @p api as the engine token for the upcoming
     *        @ref run invocation.
     *
     * The pointer is non-owning. The task must not store it past the
     * end of @ref run because the token expires when the FSM
     * transitions away from the bound state. Pass @c nullptr to clear
     * the binding (e.g. between ticks); subsequent @ref api calls
     * return @c nullptr until the next @ref setApi.
     */
    virtual void setApi(engine::IEngineToken *api) = 0;

    /**
     * @brief Returns the engine token bound for the current
     *        @ref run, or @c nullptr when no token has been bound.
     *
     * Tasks reach engine subsystems through the returned token's
     * gated and ungated accessors; see
     * @ref vigine::engine::IEngineToken for the surface. Callers
     * inside @ref run dereference without checking when the engine's
     * dispatch contract guarantees a token was bound; callers from
     * helpers running outside @ref run (e.g. event handlers) must
     * branch on null.
     */
    [[nodiscard]] virtual engine::IEngineToken *api() = 0;

    /**
     * @brief Canonical task-facing entry point.
     *
     * The task flow invokes @ref run exactly once per scheduled tick
     * after binding the engine token via @ref setApi. The returned
     * @ref Result drives the result-code-keyed transitions registered
     * on the enclosing flow: a transition with a matching
     * @ref Result::Code fires; otherwise the flow stops.
     *
     * Tasks signalling staleness (gated accessor returned
     * @ref vigine::engine::Result::Code::Expired) should return an
     * error @ref Result so the flow does not advance. The engine
     * never interrupts a running @ref run; cooperative exit is the
     * task's responsibility.
     */
    [[nodiscard]] virtual Result run() = 0;

    ITask(const ITask &)            = delete;
    ITask &operator=(const ITask &) = delete;
    ITask(ITask &&)                 = delete;
    ITask &operator=(ITask &&)      = delete;

  protected:
    ITask() = default;
};

} // namespace vigine
