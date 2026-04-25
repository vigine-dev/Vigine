#include "vigine/impl/ecs/componentmanager.h"

#include <utility>

namespace vigine
{

ComponentManager::ComponentManager() = default;

ComponentManager::~ComponentManager() = default;

Result ComponentManager::add(ComponentUPtr component)
{
    if (!component)
    {
        return Result{Result::Code::Error, "ComponentManager::add: null component"};
    }

    const ComponentKind kind = component->kind();
    if (kind == ComponentKind::Unknown)
    {
        // Unknown components fail fast so unmigrated callers see the
        // boundary instead of silently routing into a generic bucket.
        return Result{Result::Code::Error,
                      "ComponentManager::add: component reports ComponentKind::Unknown"};
    }

    _components[kind].push_back(std::move(component));
    return Result{};
}

std::vector<const IComponent *> ComponentManager::components(ComponentKind kind) const
{
    std::vector<const IComponent *> view;

    const auto it = _components.find(kind);
    if (it == _components.end())
    {
        return view;
    }

    view.reserve(it->second.size());
    for (const auto &uptr : it->second)
    {
        view.push_back(uptr.get());
    }
    return view;
}

std::size_t ComponentManager::removeAllOfKind(ComponentKind kind) noexcept
{
    const auto it = _components.find(kind);
    if (it == _components.end())
    {
        return 0;
    }

    const std::size_t released = it->second.size();
    _components.erase(it);
    return released;
}

void ComponentManager::clear() noexcept
{
    _components.clear();
}

} // namespace vigine
