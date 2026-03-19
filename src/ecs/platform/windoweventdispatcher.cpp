#include "windoweventdispatcher.h"

using namespace vigine::platform;

void WindowEventDispatcher::addHandler(IWindowEventHandlerComponent *handler)
{
    if (handler)
        _handlers.push_back(handler);
}

std::vector<IWindowEventHandlerComponent *> WindowEventDispatcher::handlers() const
{
    return _handlers;
}

void WindowEventDispatcher::onWindowClosed()
{
    for (auto *h : _handlers)
        h->onWindowClosed();
}

void WindowEventDispatcher::onWindowResized(int width, int height)
{
    for (auto *h : _handlers)
        h->onWindowResized(width, height);
}

void WindowEventDispatcher::onWindowMoved(int x, int y)
{
    for (auto *h : _handlers)
        h->onWindowMoved(x, y);
}

void WindowEventDispatcher::onWindowFocused()
{
    for (auto *h : _handlers)
        h->onWindowFocused();
}

void WindowEventDispatcher::onWindowUnfocused()
{
    for (auto *h : _handlers)
        h->onWindowUnfocused();
}

void WindowEventDispatcher::onMouseMove(int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseMove(x, y);
}

void WindowEventDispatcher::onMouseEnter()
{
    for (auto *h : _handlers)
        h->onMouseEnter();
}

void WindowEventDispatcher::onMouseLeave()
{
    for (auto *h : _handlers)
        h->onMouseLeave();
}

void WindowEventDispatcher::onMouseWheel(int delta, int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseWheel(delta, x, y);
}

void WindowEventDispatcher::onMouseHorizontalWheel(int delta, int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseHorizontalWheel(delta, x, y);
}

void WindowEventDispatcher::onMouseButtonDown(MouseButton button, int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseButtonDown(button, x, y);
}

void WindowEventDispatcher::onMouseButtonUp(MouseButton button, int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseButtonUp(button, x, y);
}

void WindowEventDispatcher::onMouseButtonDoubleClick(MouseButton button, int x, int y)
{
    for (auto *h : _handlers)
        h->onMouseButtonDoubleClick(button, x, y);
}

void WindowEventDispatcher::onKeyDown(const KeyEvent &event)
{
    for (auto *h : _handlers)
        h->onKeyDown(event);
}

void WindowEventDispatcher::onKeyUp(const KeyEvent &event)
{
    for (auto *h : _handlers)
        h->onKeyUp(event);
}

void WindowEventDispatcher::onChar(const TextEvent &event)
{
    for (auto *h : _handlers)
        h->onChar(event);
}

void WindowEventDispatcher::onDeadChar(const TextEvent &event)
{
    for (auto *h : _handlers)
        h->onDeadChar(event);
}
