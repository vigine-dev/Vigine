#include "windoweventhandler.h"

#include <iostream>

WindowEventHandler::WindowEventHandler(std::string handlerId) : _handlerId(std::move(handlerId)) {}

void WindowEventHandler::setMouseButtonDownCallback(MouseButtonDownCallback callback)
{
    _onMouseButtonDown = std::move(callback);
}

void WindowEventHandler::setKeyDownCallback(KeyDownCallback callback)
{
    _onKeyDown = std::move(callback);
}

void WindowEventHandler::onWindowClosed()
{
    std::cout << "[" << _handlerId << "] Window closed event" << std::endl;
}

void WindowEventHandler::onWindowResized(int width, int height)
{
    std::cout << "[" << _handlerId << "] Window resized: " << width << "x" << height << std::endl;
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
    std::cout << "[" << _handlerId << "] Mouse move: " << x << ", " << y << std::endl;
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
    std::cout << "[" << _handlerId << "] Mouse wheel: delta=" << delta << " at " << x << ", " << y
              << std::endl;
}

void WindowEventHandler::onMouseHorizontalWheel(int delta, int x, int y)
{
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

// COPILOT_TODO: Якщо key-up потрібен бізнес-логіці, додай окремий callback/сигнал; зараз ця подія
// лише логується і далі губиться.
void WindowEventHandler::onKeyUp(const vigine::platform::KeyEvent &event)
{
    std::cout << "[" << _handlerId
              << "] WindowEventHandler::onKeyUp: Key up: code=" << event.keyCode
              << ", scan=" << event.scanCode << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << ", isRepeat=" << event.isRepeat
              << std::endl;
}

// COPILOT_TODO: Або прокинути text input далі через callback, або явно зафіксувати, що текстові
// події цим прикладом не підтримуються.
void WindowEventHandler::onChar(const vigine::platform::TextEvent &event)
{
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
