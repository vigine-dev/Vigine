#pragma once

#include <vigine/abstracttask.h>

#include <map>
#include <string>


class ImageData;

namespace vigine
{
class Context;

namespace graphics
{
class GraphicsService;
}
} // namespace vigine

class LoadTexturesTask : public vigine::AbstractTask
{
  public:
    LoadTexturesTask() = default;

    vigine::Result execute() override;
    void contextChanged() override;

  private:
    vigine::graphics::GraphicsService *_graphicsService = nullptr;
};
