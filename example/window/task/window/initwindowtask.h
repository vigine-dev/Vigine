#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/service/serviceid.h>

#include <memory>
#include <vector>

namespace vigine
{
class EntityManager;

namespace ecs
{
namespace platform
{
class PlatformService;
}
} // namespace ecs
} // namespace vigine

/**
 * @brief Creates the main window entity and binds the platform service
 *        + a fresh @c WindowComponent + the example's event handler.
 *
 * Constructed before @c IEngine::run with the legacy @c EntityManager
 * pointer and the @ref vigine::service::ServiceId stamped for the
 * platform service at registration time. @ref run resolves the
 * platform service through @ref apiToken()->service so the lookup honours
 * the engine-token gate; on a token-expired return path the task
 * surfaces the error to the FSM transition table without touching the
 * service.
 */
class InitWindowTask final : public vigine::AbstractTask
{
  public:
    InitWindowTask();

    [[nodiscard]] vigine::Result run() override;

    void setEntityManager(vigine::EntityManager *entityManager) noexcept;
    void setPlatformServiceId(vigine::service::ServiceId id) noexcept;

  private:
    void createEventHandlers();

    vigine::EntityManager *_entityManager{nullptr};
    vigine::service::ServiceId _platformServiceId{};
    std::vector<std::unique_ptr<vigine::ecs::platform::IWindowEventHandlerComponent>> _eventHandlers;
};
