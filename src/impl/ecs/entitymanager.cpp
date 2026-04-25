#include "vigine/impl/ecs/entitymanager.h"

#include "vigine/impl/ecs/entity.h"

vigine::Entity *vigine::EntityManager::createEntity()
{
    // Stamp a fresh non-zero id so the entity can be distinguished
    // from the default-constructed sentinel that reports id() == 0.
    _entities.push_back(std::make_unique<vigine::Entity>(_nextId++));

    return _entities.back().get();
}

vigine::EntityManager::~EntityManager() = default;

void vigine::EntityManager::removeEntity(Entity *entity)
{
    std::erase_if(_entities, [entity](const EntityUPtr &uptr) { return uptr.get() == entity; });
}

void vigine::EntityManager::addAlias(Entity *entity, const std::string &alias)
{
    _entityAliases[alias] = entity;
}

vigine::Entity *vigine::EntityManager::getEntityByAlias(const std::string &alias) const
{
    if (_entityAliases.contains(alias))
        return _entityAliases.at(alias);

    return nullptr;
}

vigine::EntityManager::EntityManager() = default;
