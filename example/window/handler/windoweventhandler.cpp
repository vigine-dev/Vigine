#include "windoweventhandler.h"

#include <iostream>

WindowEventHandler::WindowEventHandler(std::string handlerId) : _handlerId(std::move(handlerId)) {}

void WindowEventHandler::setMouseButtonDownCallback(MouseButtonDownCallback callback)
{
    _onMouseButtonDown = std::move(callback);
}

void WindowEventHandler::setMouseButtonUpCallback(MouseButtonUpCallback callback)
{
    _onMouseButtonUp = std::move(callback);
}

void WindowEventHandler::setMouseMoveCallback(MouseMoveCallback callback)
{
    _onMouseMove = std::move(callback);
}

void WindowEventHandler::setMouseWheelCallback(MouseWheelCallback callback)
{
    _onMouseWheel = std::move(callback);
}

void WindowEventHandler::setKeyDownCallback(KeyDownCallback callback)
{
    _onKeyDown = std::move(callback);
}

void WindowEventHandler::setKeyUpCallback(KeyUpCallback callback)
{
    _onKeyUp = std::move(callback);
}

void WindowEventHandler::setCharCallback(CharCallback callback) { _onChar = std::move(callback); }

void WindowEventHandler::setWindowResizedCallback(WindowResizedCallback callback)
{
    _onWindowResized = std::move(callback);
}

void WindowEventHandler::setPinchGestureCallback(PinchGestureCallback callback)
{
    _onPinchGesture = std::move(callback);
}

void WindowEventHandler::setTwoFingerDragCallback(TwoFingerDragCallback callback)
{
    _onTwoFingerDrag = std::move(callback);
}

void WindowEventHandler::setMouseHWheelCallback(MouseHWheelCallback callback)
{
    _onMouseHWheel = std::move(callback);
}

void WindowEventHandler::onWindowClosed()
{
    std::cout << "[" << _handlerId << "] Window closed event" << std::endl;
}

void WindowEventHandler::onWindowResized(int width, int height)
{
    if (_onWindowResized)
        _onWindowResized(width, height);
}

void WindowEventHandler::onWindowMoved(int x, int y)
{
    std::cout << "[" << _handlerId << "] Window moved: " << x << ", " << y << std::endl;
}

void WindowEventHandler::onWindowFocused()
{
    std::cout << "[" << _handlerId << "] Window focused event" << std::endl;
}

void WindowEventHandler::onWindowUnfocused()
{
    std::cout << "[" << _handlerId << "] Window unfocused event" << std::endl;
}

void WindowEventHandler::onMouseMove(int x, int y)
{
    if (_onMouseMove)
        _onMouseMove(x, y);
}

void WindowEventHandler::onMouseEnter()
{
    std::cout << "[" << _handlerId << "] Mouse enter" << std::endl;
}

void WindowEventHandler::onMouseLeave()
{
    std::cout << "[" << _handlerId << "] Mouse leave" << std::endl;
}

void WindowEventHandler::onMouseWheel(int delta, int x, int y)
{
    if (_onMouseWheel)
        _onMouseWheel(delta, x, y);
}

void WindowEventHandler::onMouseHorizontalWheel(int delta, int x, int y)
{
    if (_onMouseHWheel)
    {
        _onMouseHWheel(delta, x, y);
        return;
    }
    std::cout << "[" << _handlerId << "] Mouse h-wheel: delta=" << delta << " at " << x << ", " << y
              << std::endl;
}

void WindowEventHandler::onMouseButtonDown(vigine::platform::MouseButton button, int x, int y)
{
    std::cout << "[" << _handlerId << "] WindowEventHandler::onMouseButtonDown: Mouse button down: "
              << static_cast<int>(button) << " at " << x << ", " << y << std::endl;

    if (_onMouseButtonDown)
        _onMouseButtonDown(button, x, y);
}

void WindowEventHandler::onMouseButtonUp(vigine::platform::MouseButton button, int x, int y)
{
    std::cout << "[" << _handlerId << "] WindowEventHandler::onMouseButtonUp: Mouse button up: "
              << static_cast<int>(button) << " at " << x << ", " << y << std::endl;

    if (_onMouseButtonUp)
        _onMouseButtonUp(button, x, y);
}

void WindowEventHandler::onMouseButtonDoubleClick(vigine::platform::MouseButton button, int x,
                                                  int y)
{
    std::cout << "[" << _handlerId
              << "] WindowEventHandler::onMouseButtonDoubleClick: Mouse button dbl-click: "
              << static_cast<int>(button) << " at " << x << ", " << y << std::endl;
}

void WindowEventHandler::onKeyDown(const vigine::platform::KeyEvent &event)
{
    std::cout << "[" << _handlerId
              << "] WindowEventHandler::onKeyDown: Key down: code=" << event.keyCode
              << ", scan=" << event.scanCode << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << ", isRepeat=" << event.isRepeat
              << std::endl;

    if (_onKeyDown)
        _onKeyDown(event);
}

void WindowEventHandler::onKeyUp(const vigine::platform::KeyEvent &event)
{
    std::cout << "[" << _handlerId
              << "] WindowEventHandler::onKeyUp: Key up: code=" << event.keyCode
              << ", scan=" << event.scanCode << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << ", isRepeat=" << event.isRepeat
              << std::endl;

    if (_onKeyUp)
        _onKeyUp(event);
}

void WindowEventHandler::onChar(const vigine::platform::TextEvent &event)
{
    if (_onChar)
    {
        _onChar(event);
        return;
    }
    std::cout << "[" << _handlerId << "] WindowEventHandler::onChar: Char: cp=" << event.codePoint
              << ", mods=" << event.modifiers << ", repeatCount=" << event.repeatCount << std::endl;
}

// COPILOT_TODO: Обробка dead-char зараз теж зводиться до логування; якщо потрібен IME/input
// pipeline, тут бракує реального dispatch.
void WindowEventHandler::onDeadChar(const vigine::platform::TextEvent &event)
{
    std::cout << "[" << _handlerId
              << "] WindowEventHandler::onDeadChar: Dead char: cp=" << event.codePoint
              << ", mods=" << event.modifiers << ", repeatCount=" << event.repeatCount << std::endl;
}

void WindowEventHandler::onPinchGesture(float scale, int centerX, int centerY)
{
    if (_onPinchGesture)
        _onPinchGesture(scale, centerX, centerY);
}

void WindowEventHandler::onTwoFingerDrag(int deltaX, int deltaY, int x, int y)
{
    if (_onTwoFingerDrag)
        _onTwoFingerDrag(deltaX, deltaY, x, y);
}
