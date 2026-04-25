#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
namespace ecs
{
namespace graphics
{
class GraphicsService;
}
} // namespace ecs
} // namespace vigine
class SetupHelperGeometryTask : public vigine::AbstractTask
{
  public:
    SetupHelperGeometryTask();

    void contextChanged() override;
    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
};
