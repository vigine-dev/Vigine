#pragma once

#include <vigine/abstracttask.h>

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

    vigine::Result execute() override;
    void contextChanged() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService = nullptr;
};
