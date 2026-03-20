#pragma once

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>

#include <memory>
#include <vector>

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
    std::vector<std::unique_ptr<vigine::platform::IWindowEventHandlerComponent>> _eventHandlers;

    void createEventHandlers();
};
