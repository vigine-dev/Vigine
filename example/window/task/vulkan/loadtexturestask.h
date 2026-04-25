#pragma once

#include <vigine/abstracttask.h>

#include <map>
#include <string>


struct ImageData;

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
class LoadTexturesTask : public vigine::AbstractTask
{
  public:
    LoadTexturesTask() = default;

    vigine::Result execute() override;
    void contextChanged() override;

  private:
    vigine::ecs::graphics::GraphicsService *_graphicsService = nullptr;
};
