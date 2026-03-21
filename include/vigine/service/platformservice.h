#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"
#include "vigine/ecs/platform/iwindoweventhandler.h"
#include "vigine/result.h"

#include <vector>

namespace vigine
{
namespace platform
{
class WindowSystem;
class IWindowEventHandlerComponent;
class WindowComponent;

class PlatformService : public AbstractService
{
  public:
    PlatformService(const Name &name);
    ~PlatformService() override;

    [[nodiscard]] ServiceId id() const override;
    WindowComponent *createWindow();
    [[nodiscard]] vigine::Result showWindow(WindowComponent *window);
    [[nodiscard]] vigine::Result bindWindowEventHandler(WindowComponent *window,
                                                        IWindowEventHandlerComponent *handler);
    [[nodiscard]] void *nativeWindowHandle(WindowComponent *window) const;
    [[nodiscard]] std::vector<WindowComponent *> windowComponents() const;
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *> windowEventHandlers() const;
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *>
    windowEventHandlers(WindowComponent *window) const;

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
