#include "windowcomponent.h"

using namespace vigine::platform;

WindowComponent::WindowComponent() {}

WindowComponent::~WindowComponent() = default;

void WindowComponent::show() {}

void WindowComponent::setEventHandler(IWindowEventHandler *handler) { _eventHandler = handler; }
