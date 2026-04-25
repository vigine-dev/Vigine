#pragma once

#include <vigine/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>

#include <memory>
#include <vector>

namespace vigine
{
namespace ecs
{
namespace platform
{
class PlatformService;
}
} // namespace ecs
} // namespace vigine
class InitWindowTask : public vigine::AbstractTask
{
  public:
    InitWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::ecs::platform::PlatformService *_platformService{nullptr};
    std::vector<std::unique_ptr<vigine::ecs::platform::IWindowEventHandlerComponent>> _eventHandlers;

    void createEventHandlers();
};
