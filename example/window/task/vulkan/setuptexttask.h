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
class SetupTextTask : public vigine::AbstractTask
{
  public:
    SetupTextTask();

    void contextChanged() override;
    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
};
