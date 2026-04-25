#include "vigine/impl/ecs/platform/platformservice.h"

#include "vigine/api/context/icontext.h"
#include "vigine/impl/ecs/platform/windowsystem.h"

#include "impl/ecs/platform/windowcomponent.h"

using namespace vigine::ecs::platform;

PlatformService::PlatformService(const Name &name)
    : vigine::service::AbstractService()
    , _name{name}
{
}

PlatformService::~PlatformService() = default;

const vigine::Name &PlatformService::name() const noexcept { return _name; }

void PlatformService::setWindowSystem(WindowSystem *system) noexcept
{
    _windowSystem = system;
}

vigine::Result PlatformService::onInit(vigine::IContext &context)
{
    // Modern lifecycle: chain to the wrapper base so the
    // @c isInitialised flag flips to @c true. Window-system
    // attachment is performed through @ref setWindowSystem because
    // @ref vigine::IContext does not yet expose a system locator.
    return vigine::service::AbstractService::onInit(context);
}

vigine::Result PlatformService::onShutdown(vigine::IContext &context)
{
    // Drop the non-owning window-system handle before chaining up.
    _windowSystem = nullptr;
    return vigine::service::AbstractService::onShutdown(context);
}

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

vigine::Result PlatformService::bindWindowEventHandler(vigine::Entity *entity, WindowComponent *window,
                                                       IWindowEventHandlerComponent *handler)
{
    if (!_windowSystem || !entity || !window)
        return vigine::Result(vigine::Result::Code::Error, "No entity or window system");

    if (auto bindWindowResult = _windowSystem->bindWindowComponent(entity, window);
        bindWindowResult.isError())
        return bindWindowResult;

    return _windowSystem->bindWindowEventHandler(entity, window, handler);
}

void *PlatformService::nativeWindowHandle(WindowComponent *window) const
{
    if (!window)
        return nullptr;

    return window->nativeHandle();
}

std::vector<WindowComponent *> PlatformService::windowComponents(vigine::Entity *entity) const
{
    if (!_windowSystem || !entity)
        return {};

    return _windowSystem->windowComponents(entity);
}

std::vector<IWindowEventHandlerComponent *> PlatformService::windowEventHandlers(vigine::Entity *entity) const
{
    if (!_windowSystem || !entity)
        return {};

    return _windowSystem->windowEventHandlers(entity, nullptr);
}

std::vector<IWindowEventHandlerComponent *>
PlatformService::windowEventHandlers(vigine::Entity *entity, WindowComponent *window) const
{
    if (!_windowSystem || !entity || !window)
        return {};

    return _windowSystem->windowEventHandlers(entity, window);
}
