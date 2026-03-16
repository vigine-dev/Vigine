#include "windoweventhandler.h"

#include <iostream>

void WindowEventHandler::onWindowClosed() { std::cout << "Window closed event" << std::endl; }

void WindowEventHandler::onWindowResized(int width, int height)
{
    std::cout << "Window resized: " << width << "x" << height << std::endl;
}

void WindowEventHandler::onWindowMoved(int x, int y)
{
    std::cout << "Window moved: " << x << ", " << y << std::endl;
}

void WindowEventHandler::onWindowFocused() { std::cout << "Window focused event" << std::endl; }

void WindowEventHandler::onWindowUnfocused() { std::cout << "Window unfocused event" << std::endl; }

void WindowEventHandler::onMouseMove(int x, int y)
{
    std::cout << "Mouse move: " << x << ", " << y << std::endl;
}

void WindowEventHandler::onMouseEnter() { std::cout << "Mouse enter" << std::endl; }

void WindowEventHandler::onMouseLeave() { std::cout << "Mouse leave" << std::endl; }

void WindowEventHandler::onMouseWheel(int delta, int x, int y)
{
    std::cout << "Mouse wheel: delta=" << delta << " at " << x << ", " << y << std::endl;
}

void WindowEventHandler::onMouseHorizontalWheel(int delta, int x, int y)
{
    std::cout << "Mouse h-wheel: delta=" << delta << " at " << x << ", " << y << std::endl;
}

void WindowEventHandler::onMouseButtonDown(vigine::platform::MouseButton button, int x, int y)
{
    std::cout << "Mouse button down: " << static_cast<int>(button) << " at " << x << ", " << y
              << std::endl;
}

void WindowEventHandler::onMouseButtonUp(vigine::platform::MouseButton button, int x, int y)
{
    std::cout << "Mouse button up: " << static_cast<int>(button) << " at " << x << ", " << y
              << std::endl;
}

void WindowEventHandler::onMouseButtonDoubleClick(vigine::platform::MouseButton button, int x,
                                                  int y)
{
    std::cout << "Mouse button dbl-click: " << static_cast<int>(button) << " at " << x << ", " << y
              << std::endl;
}

void WindowEventHandler::onKeyDown(const vigine::platform::KeyEvent &event)
{
    std::cout << "Key down: code=" << event.keyCode << ", scan=" << event.scanCode
              << ", mods=" << event.modifiers << ", repeatCount=" << event.repeatCount
              << ", isRepeat=" << event.isRepeat << std::endl;
}

void WindowEventHandler::onKeyUp(const vigine::platform::KeyEvent &event)
{
    std::cout << "Key up: code=" << event.keyCode << ", scan=" << event.scanCode
              << ", mods=" << event.modifiers << ", repeatCount=" << event.repeatCount
              << ", isRepeat=" << event.isRepeat << std::endl;
}

void WindowEventHandler::onChar(const vigine::platform::TextEvent &event)
{
    std::cout << "Char: cp=" << event.codePoint << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << std::endl;
}

void WindowEventHandler::onDeadChar(const vigine::platform::TextEvent &event)
{
    std::cout << "Dead char: cp=" << event.codePoint << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << std::endl;
}
