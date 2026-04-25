#include "vigine/api/ecs/abstractecs.h"

#include <memory>
#include <utility>
#include <vector>

#include "ecs/entityworld.h"
#include "vigine/api/ecs/ecstypes.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/result.h"

namespace vigine::ecs
{

AbstractECS::AbstractECS()
    : _world{std::make_unique<EntityWorld>()}
{
}

AbstractECS::~AbstractECS() = default;

// ---------------------------------------------------------------------------
// Protected accessors — the derived classes reach the internal world
// through these so the substrate stays invisible on the wrapper's
// public surface.
// ---------------------------------------------------------------------------

EntityWorld &AbstractECS::world() noexcept
{
    return *_world;
}

const EntityWorld &AbstractECS::world() const noexcept
{
    return *_world;
}

// ---------------------------------------------------------------------------
// IECS: entity lifecycle. Each delegation is a one-liner; the world does
// the substrate translation so the wrapper stays thin.
// ---------------------------------------------------------------------------

EntityId AbstractECS::createEntity()
{
    return _world->createEntity();
}

Result AbstractECS::removeEntity(EntityId entity)
{
    return _world->removeEntity(entity);
}

bool AbstractECS::hasEntity(EntityId entity) const noexcept
{
    return _world->hasEntity(entity);
}

// ---------------------------------------------------------------------------
// IECS: component lifecycle.
// ---------------------------------------------------------------------------

ComponentHandle AbstractECS::attachComponent(
    EntityId entity, std::unique_ptr<IComponent> component)
{
    return _world->attachComponent(entity, std::move(component));
}

Result AbstractECS::detachComponent(EntityId entity, ComponentTypeId typeId)
{
    return _world->detachComponent(entity, typeId);
}

const IComponent *AbstractECS::findComponent(
    EntityId entity, ComponentTypeId typeId) const
{
    return _world->findComponent(entity, typeId);
}

std::vector<const IComponent *> AbstractECS::componentsOf(EntityId entity) const
{
    return _world->componentsOf(entity);
}

// ---------------------------------------------------------------------------
// IECS: bulk query.
// ---------------------------------------------------------------------------

std::vector<EntityId> AbstractECS::entitiesWith(ComponentTypeId typeId) const
{
    return _world->entitiesWith(typeId);
}

} // namespace vigine::ecs
