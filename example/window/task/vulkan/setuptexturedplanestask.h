#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
class Context;

namespace ecs
{
namespace graphics
{
class GraphicsService;
}
} // namespace ecs
} // namespace vigine
class SetupTexturedPlanesTask : public vigine::AbstractTask
{
  public:
    SetupTexturedPlanesTask() = default;

    [[nodiscard]] vigine::Result run() override;
    void contextChanged() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService = nullptr;
};
