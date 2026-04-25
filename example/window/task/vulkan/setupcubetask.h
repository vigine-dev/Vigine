#pragma once

#include <vigine/abstracttask.h>

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
class SetupCubeTask : public vigine::AbstractTask
{
  public:
    SetupCubeTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
};
