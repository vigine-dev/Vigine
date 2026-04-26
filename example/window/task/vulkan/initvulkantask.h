#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/service/serviceid.h>

namespace vigine
{
class EntityManager;

namespace ecs
{
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
namespace platform
{
class PlatformService;
}
} // namespace ecs
} // namespace vigine

class InitVulkanTask final : public vigine::AbstractTask
{
  public:
    InitVulkanTask();

    [[nodiscard]] vigine::Result run() override;

    void setEntityManager(vigine::EntityManager *entityManager) noexcept;
    void setPlatformServiceId(vigine::service::ServiceId id) noexcept;
    void setGraphicsServiceId(vigine::service::ServiceId id) noexcept;

  private:
    vigine::EntityManager *_entityManager{nullptr};
    vigine::service::ServiceId _platformServiceId{};
    vigine::service::ServiceId _graphicsServiceId{};
};
