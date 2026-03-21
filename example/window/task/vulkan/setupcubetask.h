#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
namespace graphics
{
class GraphicsService;
}
} // namespace vigine

class SetupCubeTask : public vigine::AbstractTask
{
  public:
    SetupCubeTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::graphics::GraphicsService *_graphicsService{nullptr};
};
