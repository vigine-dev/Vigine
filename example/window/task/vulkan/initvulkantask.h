#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
namespace ecs
{
namespace graphics
{
class RenderSystem;
}
namespace platform
{
class PlatformService;
}
} // namespace ecs
} // namespace vigine
class InitVulkanTask : public vigine::AbstractTask
{
  public:
    InitVulkanTask();

    void contextChanged() override;
    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::ecs::graphics::RenderSystem *_renderSystem{nullptr};
    vigine::ecs::platform::PlatformService *_platformService{nullptr};
};
