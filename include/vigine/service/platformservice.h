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

    [[nodiscard]] ServiceId id() const override;
    void createWindow();
    void showWindow();

  protected:
    void contextChanged() override;
    void entityBound() override;
    void entityUnbound() override;

  private:
    WindowSystem *_windowSystem{nullptr};
};

BUILD_SMART_PTR(PlatformService);

} // namespace platform
} // namespace vigine
