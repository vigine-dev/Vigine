#pragma once

#include "windoweventsignal.h"

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>

#include <chrono>
#include <cstdint>

namespace vigine
{
namespace platform
{
class PlatformService;
class WindowComponent;
} // namespace platform
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
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
    void onMouseButtonUp(vigine::platform::MouseButton button, int x, int y);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(const vigine::platform::KeyEvent &event);
    void onKeyUp(const vigine::platform::KeyEvent &event);

  private:
    void onWindowResized(vigine::platform::WindowComponent *window, int width, int height);
    void updateCameraMovementKey(unsigned int keyCode, bool pressed);

    vigine::platform::PlatformService *_platformService{nullptr};
    vigine::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::graphics::RenderSystem *_renderSystem{nullptr};
    vigine::platform::WindowComponent *_pendingResizeWindow{nullptr};
    uint32_t _pendingResizeWidth{0};
    uint32_t _pendingResizeHeight{0};
    uint32_t _appliedResizeWidth{0};
    uint32_t _appliedResizeHeight{0};
    bool _resizePending{false};
    std::chrono::steady_clock::time_point _lastResizeEvent{};
    std::chrono::steady_clock::time_point _lastResizeApply{};
};
