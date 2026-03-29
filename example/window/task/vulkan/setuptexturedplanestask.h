#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
class Context;

namespace graphics
{
class GraphicsService;
}
} // namespace vigine

class SetupTexturedPlanesTask : public vigine::AbstractTask
{
  public:
    SetupTexturedPlanesTask() = default;

    vigine::Result execute() override;
    void contextChanged() override;

  private:
    vigine::graphics::GraphicsService *_graphicsService = nullptr;
};
