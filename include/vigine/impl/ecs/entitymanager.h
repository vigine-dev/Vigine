#pragma once

/**
 * @file entitymanager.h
 * @brief Legacy concrete EntityManager that owns Entity lifetimes.
 */

#include "vigine/api/ecs/abstractentitymanager.h"
#include "vigine/api/ecs/ientity.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vigine
{
class Engine;
class Entity;

using EntityUPtr = std::unique_ptr<Entity>;

/**
 * @brief Owns the live set of Entity objects and their string aliases.
 *
 * @ref createEntity allocates a new Entity, stamps it with a fresh
 * monotonic @ref IEntity::Id, and keeps it alive until
 * @ref removeEntity releases it. Optional string aliases registered
 * via @ref addAlias allow lookup by @ref getEntityByAlias. Instances
 * are constructed by @ref Engine.
 *
 * Closes the legacy entity manager chain: derives from
 * @ref AbstractEntityManager, which in turn implements the pure
 * @ref IEntityManager contract.
 */
class EntityManager final : public AbstractEntityManager
{
  public:
    ~EntityManager() override;
    Entity *createEntity();
    void removeEntity(Entity *entity);
    void addAlias(Entity *entity, const std::string &alias);
    Entity *getEntityByAlias(const std::string &alias) const;

  private:
    EntityManager();
    std::vector<EntityUPtr> _entities;
    std::map<std::string, Entity *> _entityAliases;
    /**
     * @brief Monotonic id counter handed out by @ref createEntity.
     *
     * Starts at @c 1 so a stamped id is always non-zero and therefore
     * distinguishable from the @c 0 sentinel a default-constructed
     * entity reports until it is registered.
     */
    IEntity::Id _nextId{1};

    friend class Engine;
};
} // namespace vigine
