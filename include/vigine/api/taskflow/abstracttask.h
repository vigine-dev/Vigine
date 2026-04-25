#pragma once

/**
 * @file abstracttask.h
 * @brief Stateful base class for a single unit of work run inside a
 *        TaskFlow.
 */

#include "vigine/api/taskflow/itask.h"
#include "vigine/result.h"

namespace vigine::engine
{
class IEngineToken;
} // namespace vigine::engine

namespace vigine
{

class Context;

/**
 * @brief Stateful @ref ITask base.
 *
 * @ref AbstractTask wires the task-side half of the
 * @ref vigine::engine::IEngineToken contract documented in
 * @c architecture.md § R-StateScope: it stores the non-owning token
 * pointer bound by the engine via @ref setApi and exposes it through
 * @ref api. Concrete tasks override @ref run; they reach engine
 * subsystems through @ref api or through the legacy @ref context
 * accessor while the umbrella branch is still mid-migration to a
 * token-only world.
 *
 * Composition, not inheritance:
 *   - The base inherits from @ref ITask only. The previous
 *     @c ContextHolder mixin has been deleted (issue #283); the
 *     @c Context* pointer is now held as a private member and
 *     surfaced through @ref setContext / @ref context /
 *     @ref contextChanged for tasks that have not yet been migrated
 *     off direct context access.
 *
 * Strict encapsulation: the @c _api token pointer and the
 * @c _context pointer are both @c private. The @ref setApi and
 * @ref api overrides are marked @c final so concrete tasks cannot
 * bypass the binding contract.
 */
class AbstractTask : public ITask
{
  public:
    ~AbstractTask() override;

    void setApi(engine::IEngineToken *api) noexcept override final;

    [[nodiscard]] engine::IEngineToken *api() noexcept override final;

    void setContext(Context &context);

  protected:
    AbstractTask();

    Context *context() const;
    virtual void contextChanged();

  private:
    /**
     * @brief Non-owning context pointer bound externally via
     *        @ref setContext.
     *
     * Held by composition; the previous @c ContextHolder mixin has
     * been deleted (issue #283). Concrete tasks that have not yet
     * been migrated off direct context access reach the context via
     * @ref context.
     */
    Context *_context{nullptr};

    /**
     * @brief Non-owning engine token bound by the task flow before
     *        every @ref run invocation.
     *
     * Reset to @c nullptr when the task flow clears the binding
     * between ticks. The pointer never owns the underlying token; the
     * task flow (or the state machine that issued the token) keeps
     * the concrete object alive while it is in scope.
     */
    engine::IEngineToken *_api{nullptr};
};

} // namespace vigine
