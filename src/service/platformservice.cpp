#include "vigine/service/platformservice.h"

#include "vigine/context.h"
#include "vigine/ecs/platform/windowsystem.h"
#include "vigine/property.h"

using namespace vigine::platform;

PlatformService::PlatformService(const Name &name) : AbstractService(name) {}

PlatformService::~PlatformService() = default;

void PlatformService::createWindow()
{
    auto *entity = getBoundEntity();

    if (!_windowSystem || !entity)
        return;

    if (!_windowSystem->hasComponents(entity))
        _windowSystem->createComponents(entity);

    _windowSystem->bindEntity(entity);
}

void PlatformService::showWindow()
{
    if (_windowSystem)
        _windowSystem->showWindow();
}

void PlatformService::setWindowEventHandler(IWindowEventHandler *handler)
{
    _windowEventHandler = handler;

    if (_windowSystem)
        _windowSystem->setWindowEventHandler(handler);
}

IWindowEventHandler *PlatformService::windowEventHandler() const { return _windowEventHandler; }

void PlatformService::contextChanged()
{
    if (!context())
    {
        _windowSystem = nullptr;

        return;
    }

    _windowSystem = dynamic_cast<WindowSystem *>(
        context()->system("Window", "MainWindow", vigine::Property::Exist));

    if (_windowSystem)
        return;

    _windowSystem = dynamic_cast<WindowSystem *>(
        context()->system("Window", "MainWindow", vigine::Property::New));
}

void PlatformService::entityBound() { createWindow(); }

void PlatformService::entityUnbound()
{
    if (_windowSystem)
        _windowSystem->unbindEntity();
}

vigine::ServiceId PlatformService::id() const { return "Platform"; }
