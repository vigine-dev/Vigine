#pragma once

#include <vigine/api/statemachine/stateid.h>
#include <vigine/api/taskflow/abstracttask.h>

namespace vigine::engine
{
class IEngine;
} // namespace vigine::engine

namespace vigine::statemachine
{
class IStateMachine;
} // namespace vigine::statemachine

// ---------------------------------------------------------------------------
// Small helper tasks that bridge the in-flow Result chain into the FSM
// API. The post-#334 engine only pumps the TaskFlow that is bound to
// the FSM's current state; advancing between states therefore needs an
// explicit `requestTransition` (queued; the engine drains it on the
// next pump tick) or, for the close phase, an `Engine::shutdown` call.
// ---------------------------------------------------------------------------

/**
 * @brief Asks the FSM to switch to a target state, then returns Success.
 *
 * The task is added as the terminal element of a per-state TaskFlow
 * after every domain task that needed to run in that state. A successful
 * domain chain reaches this task, which queues the transition through
 * `IStateMachine::requestTransition`. The engine's tick loop drains the
 * queue right after pumping the bound flow, so the next pump iteration
 * picks up the TaskFlow registered against the new state.
 */
class TransitionTask final : public vigine::AbstractTask
{
  public:
    TransitionTask(vigine::statemachine::IStateMachine *stateMachine,
                   vigine::statemachine::StateId        target);

    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::statemachine::IStateMachine *_stateMachine;
    vigine::statemachine::StateId        _target;
};

/**
 * @brief Asks the engine to stop the main loop, then returns Success.
 *
 * Used as the only task on the close-state TaskFlow: once the FSM has
 * reached close, the demo has nothing more to do, so the task calls
 * `IEngine::shutdown` and the next pump tick observes the request and
 * returns from `Engine::run`.
 */
class ShutdownTask final : public vigine::AbstractTask
{
  public:
    explicit ShutdownTask(vigine::engine::IEngine *engine);

    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::engine::IEngine *_engine;
};
