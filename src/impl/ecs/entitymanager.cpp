#include "vigine/impl/ecs/entitymanager.h"

#include "vigine/impl/ecs/entity.h"

vigine::EntityManager::EntityManager() = default;

vigine::EntityManager::~EntityManager() = default;

vigine::Entity *vigine::EntityManager::createEntity()
{
    // Stamp a fresh non-zero id so the entity can be distinguished
    // from the default-constructed sentinel that reports id() == 0.
    _entities.push_back(std::make_unique<vigine::Entity>(_nextId++));

    return _entities.back().get();
}

void vigine::EntityManager::removeEntity(IEntity *entity)
{
    if (entity == nullptr)
        return;

    // Drop alias bindings first so a future @ref getEntityByAlias
    // call cannot hand out a pointer that is about to be deleted by
    // the @ref _entities erase below. The check uses the IEntity*
    // identity so callers that pass through the interface signature
    // are matched the same way as those that pass an Entity* directly.
    for (auto it = _entityAliases.begin(); it != _entityAliases.end();)
    {
        if (static_cast<IEntity *>(it->second) == entity)
            it = _entityAliases.erase(it);
        else
            ++it;
    }

    std::erase_if(_entities, [entity](const EntityUPtr &uptr) {
        return static_cast<IEntity *>(uptr.get()) == entity;
    });
}

void vigine::EntityManager::addAlias(IEntity *entity, const std::string &alias)
{
    // A null entity with an active alias drops the binding rather
    // than storing a null entry; this keeps @ref getEntityByAlias's
    // "null on miss" contract observable from the call site without
    // a poisoned mapping ever materialising.
    if (entity == nullptr)
    {
        _entityAliases.erase(alias);
        return;
    }

    // Manager only ever stores entities it allocated through
    // @ref createEntity, so a pointer the user just got back from
    // this manager is necessarily an @c Entity * underneath. The
    // static_cast is safe by construction; a future leaf may swap it
    // for a checked dynamic_cast if the manager grows alternative
    // entity types.
    _entityAliases[alias] = static_cast<Entity *>(entity);
}

vigine::Entity *vigine::EntityManager::getEntityByAlias(const std::string &alias) const
{
    if (_entityAliases.contains(alias))
        return _entityAliases.at(alias);

    return nullptr;
}
