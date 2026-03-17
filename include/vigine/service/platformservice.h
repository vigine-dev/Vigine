#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"
#include "vigine/ecs/platform/iwindoweventhandler.h"

namespace vigine
{
namespace platform
{
class WindowSystem;
class IWindowEventHandler;

class PlatformService : public AbstractService
{
  public:
    PlatformService(const Name &name);
    ~PlatformService() override;

    [[nodiscard]] ServiceId id() const override;
    void createWindow();
    void showWindow();
    void setWindowEventHandler(IWindowEventHandler *handler);
    [[nodiscard]] IWindowEventHandler *windowEventHandler() const;

  protected:
    void contextChanged() override;
    void entityBound() override;
    void entityUnbound() override;

  private:
    WindowSystem *_windowSystem{nullptr};
    IWindowEventHandler *_windowEventHandler{nullptr};
};

BUILD_SMART_PTR(PlatformService);

} // namespace platform
} // namespace vigine
