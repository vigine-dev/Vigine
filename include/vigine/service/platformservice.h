#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"

namespace vigine
{
namespace platform
{
class WindowSystem;

class PlatformService : public AbstractService
{
  public:
    PlatformService(const Name &name);
    ~PlatformService() override;

    ServiceId id() const override;

  protected:
    void contextChanged() override;
    void entityBound() override;

  private:
    WindowSystem *_windowSystem{nullptr};
};

BUILD_SMART_PTR(PlatformService);

} // namespace platform
} // namespace vigine
