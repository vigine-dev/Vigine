#include "vigine/ecs/entitymanager.h"

#include "vigine/ecs/entity.h"

#include <algorithm>

vigine::Entity *vigine::EntityManager::createEntity()
{
    _entities.push_back(std::make_unique<vigine::Entity>());

    return _entities.back().get();
}

vigine::EntityManager::~EntityManager() {}

void vigine::EntityManager::removeEntity(Entity *entity)
{
    std::erase_if(_entities, [entity](const EntityUPtr &uptr) { return uptr.get() == entity; });
}

vigine::EntityManager::EntityManager() {}
