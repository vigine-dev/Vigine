#pragma once

/**
 * @file abstracttask.h
 * @brief Stateful base class for a single unit of work run inside a
 *        task flow.
 */

#include "vigine/api/taskflow/itask.h"
#include "vigine/result.h"

namespace vigine::engine
{
class IEngineToken;
} // namespace vigine::engine

namespace vigine
{

/**
 * @brief Stateful @ref ITask base.
 *
 * @ref AbstractTask wires the task-side half of the
 * @ref vigine::engine::IEngineToken contract documented in
 * @c architecture.md § R-StateScope: it stores the non-owning token
 * pointer bound by the engine via @ref setApiToken and exposes it through
 * @ref apiToken. Concrete tasks override @ref run and reach engine
 * subsystems exclusively through the bound token returned by
 * @ref apiToken.
 *
 * Strict encapsulation: the @c _api token pointer is @c private. The
 * @ref setApiToken and @ref apiToken overrides are marked @c final so concrete
 * tasks cannot bypass the binding contract.
 */
class AbstractTask : public ITask
{
  public:
    ~AbstractTask() override;

    void setApiToken(engine::IEngineToken *api) noexcept override final;

    [[nodiscard]] engine::IEngineToken *apiToken() noexcept override final;

  protected:
    AbstractTask();

  private:
    /**
     * @brief Non-owning engine token bound by the task flow before
     *        every @ref run invocation.
     *
     * Reset to @c nullptr when the task flow clears the binding
     * between ticks. The pointer never owns the underlying token; the
     * task flow (or the state machine that issued the token) keeps
     * the concrete object alive while it is in scope.
     */
    engine::IEngineToken *_apiToken{nullptr};
};

} // namespace vigine
