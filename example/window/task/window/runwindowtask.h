#pragma once

#include "windoweventsignal.h"

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>

namespace vigine
{
namespace platform
{
class PlatformService;
}
} // namespace vigine

class RunWindowTask : public vigine::AbstractTask,
                      public IMouseEventSignalEmiter,
                      public IKeyEventSignalEmiter
{
  public:
    RunWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

    void onMouseButtonDown(vigine::platform::MouseButton button, int x, int y);
    void onKeyDown(const vigine::platform::KeyEvent &event);

  private:
    vigine::platform::PlatformService *_platformService{nullptr};
};
