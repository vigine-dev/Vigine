#pragma once

#include "vigine/api/ecs/platform/iwindoweventhandler.h"

#include <vector>

namespace vigine
{
namespace ecs
{
namespace platform
{

// Internal system-level dispatcher: set as the actual event handler on a WindowComponent.
// Holds a list of external IWindowEventHandlerComponent* and forwards every event to all of them.
class WindowEventDispatcher : public IWindowEventHandlerComponent
{
  public:
    void addHandler(IWindowEventHandlerComponent *handler);
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *> handlers() const;

    // IWindowEventHandlerComponent
    void onWindowClosed() override;
    void onWindowResized(int width, int height) override;
    void onWindowMoved(int x, int y) override;
    void onWindowFocused() override;
    void onWindowUnfocused() override;

    void onMouseMove(int x, int y) override;
    void onMouseEnter() override;
    void onMouseLeave() override;
    void onMouseWheel(int delta, int x, int y) override;
    void onMouseHorizontalWheel(int delta, int x, int y) override;
    void onMouseButtonDown(MouseButton button, int x, int y) override;
    void onMouseButtonUp(MouseButton button, int x, int y) override;
    void onMouseButtonDoubleClick(MouseButton button, int x, int y) override;

    void onKeyDown(const KeyEvent &event) override;
    void onKeyUp(const KeyEvent &event) override;
    void onChar(const TextEvent &event) override;
    void onDeadChar(const TextEvent &event) override;

  private:
    std::vector<IWindowEventHandlerComponent *> _handlers;
};

} // namespace platform
} // namespace ecs
} // namespace vigine
