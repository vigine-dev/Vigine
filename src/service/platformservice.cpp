#include "vigine/service/platformservice.h"

#include "vigine/context.h"
#include "vigine/ecs/platform/windowsystem.h"
#include "vigine/property.h"

using namespace vigine::platform;

PlatformService::PlatformService(const Name &name) : AbstractService(name) {}

PlatformService::~PlatformService() = default;

WindowComponent *PlatformService::createWindow()
{
    if (!_windowSystem)
        return nullptr;

    return _windowSystem->createWindowComponent();
}

vigine::Result PlatformService::showWindow(WindowComponent *window)
{
    if (!_windowSystem)
        return vigine::Result(vigine::Result::Code::Error, "Window system is unavailable");

    return _windowSystem->showWindow(window);
}

vigine::Result PlatformService::bindWindowEventHandler(WindowComponent *window,
                                                       IWindowEventHandlerComponent *handler)
{
    auto *entity = getBoundEntity();

    if (!_windowSystem || !entity || !window)
        return vigine::Result(vigine::Result::Code::Error, "No bound entity or window system");

    if (auto bindWindowResult = _windowSystem->bindWindowComponent(entity, window);
        bindWindowResult.isError())
        return bindWindowResult;

    return _windowSystem->bindWindowEventHandler(entity, window, handler);
}

std::vector<WindowComponent *> PlatformService::windowComponents() const
{
    auto *entity = getBoundEntity();

    if (!_windowSystem || !entity)
        return {};

    return _windowSystem->windowComponents(entity);
}

std::vector<IWindowEventHandlerComponent *> PlatformService::windowEventHandlers() const
{
    auto *entity = getBoundEntity();

    if (!_windowSystem || !entity)
        return {};

    return _windowSystem->windowEventHandlers(entity, nullptr);
}

std::vector<IWindowEventHandlerComponent *>
PlatformService::windowEventHandlers(WindowComponent *window) const
{
    auto *entity = getBoundEntity();

    if (!_windowSystem || !entity || !window)
        return {};

    return _windowSystem->windowEventHandlers(entity, window);
}

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

void PlatformService::entityBound()
{
    if (_windowSystem)
        _windowSystem->bindEntity(getBoundEntity());
}

void PlatformService::entityUnbound()
{
    if (_windowSystem)
        _windowSystem->unbindEntity();
}

vigine::ServiceId PlatformService::id() const { return "Platform"; }
