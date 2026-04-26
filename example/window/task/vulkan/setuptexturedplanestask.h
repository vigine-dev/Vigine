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
}
} // namespace ecs
} // namespace vigine

class SetupTexturedPlanesTask final : public vigine::AbstractTask
{
  public:
    SetupTexturedPlanesTask() = default;

    [[nodiscard]] vigine::Result run() override;

    void setEntityManager(vigine::EntityManager *entityManager) noexcept;
    void setGraphicsServiceId(vigine::service::ServiceId id) noexcept;

  private:
    vigine::EntityManager *_entityManager{nullptr};
    vigine::service::ServiceId _graphicsServiceId{};
};
