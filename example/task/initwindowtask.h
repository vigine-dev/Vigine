#pragma once

#include <vigine/abstracttask.h>

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
};
