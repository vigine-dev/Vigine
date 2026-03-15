#include "vigine/ecs/platform/windowsystem.h"
#include "windowcomponent.h"

#ifdef _WIN32
#include "winapicomponent.h"
#endif

using namespace vigine::platform;

WindowSystem::WindowSystem(const SystemName &name)
    : AbstractSystem(name), _boundEntityComponent(nullptr)
{
}

WindowSystem::~WindowSystem() = default;

vigine::SystemId WindowSystem::id() const { return "Window"; }

bool WindowSystem::hasComponents(Entity *entity) const
{
    if (!entity || _entityComponents.empty())
        return false;

    return _entityComponents.contains(entity);
}

void WindowSystem::createComponents(Entity *entity)
{
    if (!entity)
        return;

#ifdef _WIN32
    _entityComponents[entity] = std::make_unique<WinAPIComponent>();
#else
    _entityComponents[entity] = std::make_unique<WindowComponent>();
#endif
}

void WindowSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    const auto it = _entityComponents.find(entity);
    if (it == _entityComponents.end())
        return;

    if (_boundEntityComponent == it->second.get())
        _boundEntityComponent = nullptr;

    _entityComponents.erase(entity);
}

void WindowSystem::showWindow()
{
    if (_boundEntityComponent)
        _boundEntityComponent->show();
}

void WindowSystem::entityBound()
{
    auto boundEntity      = getBoundEntity();
    _boundEntityComponent = nullptr;

    if (_entityComponents.contains(boundEntity))
        _boundEntityComponent = _entityComponents.at(boundEntity).get();
}

void WindowSystem::entityUnbound() { _boundEntityComponent = nullptr; }
