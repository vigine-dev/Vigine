#pragma once

#include <vigine/ecs/platform/iwindoweventhandler.h>

#include <functional>
#include <string>

class WindowEventHandler : public vigine::platform::IWindowEventHandlerComponent
{
  public:
    using MouseButtonDownCallback = std::function<void(vigine::platform::MouseButton, int, int)>;
    using MouseButtonUpCallback   = std::function<void(vigine::platform::MouseButton, int, int)>;
    using MouseMoveCallback       = std::function<void(int, int)>;
    using MouseWheelCallback      = std::function<void(int, int, int)>;
    using KeyDownCallback         = std::function<void(const vigine::platform::KeyEvent &)>;
    using KeyUpCallback           = std::function<void(const vigine::platform::KeyEvent &)>;
    using WindowResizedCallback   = std::function<void(int, int)>;

    explicit WindowEventHandler(std::string handlerId = "Handler");

    void setMouseButtonDownCallback(MouseButtonDownCallback callback);
    void setMouseButtonUpCallback(MouseButtonUpCallback callback);
    void setMouseMoveCallback(MouseMoveCallback callback);
    void setMouseWheelCallback(MouseWheelCallback callback);
    void setKeyDownCallback(KeyDownCallback callback);
    void setKeyUpCallback(KeyUpCallback callback);
    void setWindowResizedCallback(WindowResizedCallback callback);

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

  private:
    std::string _handlerId;
    MouseButtonDownCallback _onMouseButtonDown;
    MouseButtonUpCallback _onMouseButtonUp;
    MouseMoveCallback _onMouseMove;
    MouseWheelCallback _onMouseWheel;
    KeyDownCallback _onKeyDown;
    KeyUpCallback _onKeyUp;
    WindowResizedCallback _onWindowResized;
};
