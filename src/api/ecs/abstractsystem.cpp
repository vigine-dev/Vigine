#include "vigine/api/ecs/abstractsystem.h"

vigine::SystemName vigine::AbstractSystem::name() { return _name; }

vigine::AbstractSystem::~AbstractSystem() {}

vigine::AbstractSystem::AbstractSystem(const SystemName &name) : _name{name} {}

void vigine::AbstractSystem::bindEntity(Entity *entity)
{
    _entity = entity;
    entityBound();
}

void vigine::AbstractSystem::unbindEntity()
{
    _entity = nullptr;
    entityUnbound();
}

vigine::Entity *vigine::AbstractSystem::getBoundEntity() const { return _entity; }

void vigine::AbstractSystem::entityBound() {}

void vigine::AbstractSystem::entityUnbound() {}
