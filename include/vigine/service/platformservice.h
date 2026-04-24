#pragma once

/**
 * @file platformservice.h
 * @brief Concrete service that exposes window-platform operations
 *        (window creation, visibility, native handle access, event
 *        handler binding) through the service container.
 */

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

/**
 * @brief Platform service: owns a @c WindowSystem and mediates
 *        window lifecycle and event-handler binding for clients.
 *
 * Created by the engine and registered against a bound entity; it
 * exposes a window-centric API (create, show, query native handle,
 * bind an @c IWindowEventHandlerComponent) built on top of the
 * ECS-side @c WindowSystem. The service is not directly constructed
 * by user code; callers reach it through the service container.
 */
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
