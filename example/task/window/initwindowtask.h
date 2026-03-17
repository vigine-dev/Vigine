#pragma once

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>

#include <memory>

namespace vigine
{
namespace platform
{
class PlatformService;
}
} // namespace vigine

class InitWindowTask : public vigine::AbstractTask
{
  public:
    InitWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::platform::PlatformService *_platformService{nullptr};
    std::unique_ptr<vigine::platform::IWindowEventHandler> _eventHandler;

    void createEventHandler();
};
