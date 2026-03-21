#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
namespace graphics
{
class RenderSystem;
}
namespace platform
{
class PlatformService;
}
} // namespace vigine

class InitVulkanTask : public vigine::AbstractTask
{
  public:
    InitVulkanTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::graphics::RenderSystem *_renderSystem{nullptr};
    vigine::platform::PlatformService *_platformService{nullptr};
};
