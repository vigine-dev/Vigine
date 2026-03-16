#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
namespace platform
{
class PlatformService;
}
} // namespace vigine

class RunWindowTask : public vigine::AbstractTask
{
  public:
    RunWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::platform::PlatformService *_platformService{nullptr};
};
