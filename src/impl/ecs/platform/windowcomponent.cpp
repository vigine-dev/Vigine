#include "windowcomponent.h"

using namespace vigine::ecs::platform;

WindowComponent::WindowComponent() {}

WindowComponent::~WindowComponent() = default;

void WindowComponent::show() {}

void WindowComponent::setEventHandler(IWindowEventHandlerComponent *handler)
{
    _eventHandler = handler;
}

void WindowComponent::setFrameCallback(std::function<void()> callback)
{
    _frameCallback = std::move(callback);
}

void WindowComponent::runFrameCallback()
{
    if (_frameCallback)
        _frameCallback();
}

void *WindowComponent::nativeHandle() const { return nullptr; }

const std::function<void()> &WindowComponent::frameCallback() const { return _frameCallback; }
