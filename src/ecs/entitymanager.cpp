#include "vigine/ecs/entitymanager.h"

#include <algorithm>

vigine::Entity *vigine::EntityManager::createEntity() { return nullptr; }

vigine::Entity *vigine::EntityManager::createEntity() {}

vigine::Entity *vigine::EntityManager::createEntity() { return _entities.emplace_back().get(); }

void vigine::EntityManager::removeEntity(Entity *entity)
{
    std::erase_if(_entities, [entity](const EntityUPtr &uptr) { return uptr.get() == entity; });
}

vigine::EntityManager::EntityManager() {}
