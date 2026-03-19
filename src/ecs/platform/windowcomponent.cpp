#include "windowcomponent.h"

using namespace vigine::platform;

WindowComponent::WindowComponent() {}

WindowComponent::~WindowComponent() = default;

void WindowComponent::show() {}

void WindowComponent::setEventHandler(IWindowEventHandlerComponent *handler)
{
    _eventHandler = handler;
}
