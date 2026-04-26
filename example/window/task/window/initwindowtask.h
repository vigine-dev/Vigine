#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>

#include <memory>
#include <vector>

/**
 * @brief Creates the main window entity and binds the platform service
 *        + a fresh @c WindowComponent + the example's event handler.
 *
 * The task self-resolves every dependency through @ref apiToken
 * inside @ref run: the engine-token's entity manager (the engine's
 * default @c EntityManager) and the well-known platform service id
 * resolve every handle the task needs without anyone wiring them up
 * via setters at construction time.
 */
class InitWindowTask final : public vigine::AbstractTask
{
  public:
    InitWindowTask();

    [[nodiscard]] vigine::Result run() override;

  private:
    void createEventHandlers();

    std::vector<std::unique_ptr<vigine::ecs::platform::IWindowEventHandlerComponent>> _eventHandlers;
};
