#include "vigine/impl/ecs/platform/platformservice.h"

#include "vigine/api/context/icontext.h"
#include "vigine/impl/ecs/platform/windowsystem.h"

#include "impl/ecs/platform/windowcomponent.h"

#include <cassert>
#include <utility>

using namespace vigine::ecs::platform;

PlatformService::PlatformService(const Name &name,
                                 std::unique_ptr<WindowSystem> windowSystem)
    : vigine::service::AbstractService()
    , _name{name}
    , _windowSystem{std::move(windowSystem)}
{
    // Construction pre-condition: the service owns a non-null
    // window system for its entire lifetime so every accessor below
    // can dereference @ref _windowSystem unconditionally. The default
    // registration path in @ref AbstractContext supplies a default
    // @c WindowSystem; applications that override the default through
    // @c IContext::registerService also pass a non-null one (a null
    // service is rejected by the registry's own validation).
    assert(_windowSystem && "PlatformService: windowSystem must be non-null");
}

PlatformService::~PlatformService() = default;

const vigine::Name &PlatformService::name() const noexcept { return _name; }

WindowSystem *PlatformService::windowSystem() const noexcept
{
    return _windowSystem.get();
}

vigine::Result PlatformService::onInit(vigine::IContext &context)
{
    // Modern lifecycle: chain to the wrapper base so the
    // @c isInitialised flag flips to @c true. The window system was
    // wired in at construction time and is owned by this service for
    // its entire lifetime; @ref onInit therefore has no per-call
    // attachment work to do.
    return vigine::service::AbstractService::onInit(context);
}

vigine::Result PlatformService::onShutdown(vigine::IContext &context)
{
    // The owned @c WindowSystem is torn down alongside this service
    // via the private @c unique_ptr; @ref onShutdown only flips the
    // @c isInitialised flag back through the wrapper base.
    return vigine::service::AbstractService::onShutdown(context);
}

WindowComponent *PlatformService::createWindow()
{
    return _windowSystem->createWindowComponent();
}

vigine::Result PlatformService::showWindow(WindowComponent *window)
{
    return _windowSystem->showWindow(window);
}

vigine::Result PlatformService::bindWindowEventHandler(vigine::Entity *entity, WindowComponent *window,
                                                       IWindowEventHandlerComponent *handler)
{
    if (!entity)
        return vigine::Result(vigine::Result::Code::Error,
                              "PlatformService::bindWindowEventHandler: entity is null");
    if (!window)
        return vigine::Result(vigine::Result::Code::Error,
                              "PlatformService::bindWindowEventHandler: window is null");

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
    if (!entity)
        return {};

    return _windowSystem->windowComponents(entity);
}

std::vector<IWindowEventHandlerComponent *> PlatformService::windowEventHandlers(vigine::Entity *entity) const
{
    if (!entity)
        return {};

    return _windowSystem->windowEventHandlers(entity, nullptr);
}

std::vector<IWindowEventHandlerComponent *>
PlatformService::windowEventHandlers(vigine::Entity *entity, WindowComponent *window) const
{
    if (!entity || !window)
        return {};

    return _windowSystem->windowEventHandlers(entity, window);
}
