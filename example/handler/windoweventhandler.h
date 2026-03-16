#pragma once

#include <vigine/ecs/platform/iwindoweventhandler.h>

class WindowEventHandler : public vigine::platform::IWindowEventHandler
{
  public:
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
    void onMouseButtonDown(vigine::platform::MouseButton button, int x, int y) override;
    void onMouseButtonUp(vigine::platform::MouseButton button, int x, int y) override;
    void onMouseButtonDoubleClick(vigine::platform::MouseButton button, int x, int y) override;

    void onKeyDown(const vigine::platform::KeyEvent &event) override;
    void onKeyUp(const vigine::platform::KeyEvent &event) override;
    void onChar(const vigine::platform::TextEvent &event) override;
    void onDeadChar(const vigine::platform::TextEvent &event) override;
};
