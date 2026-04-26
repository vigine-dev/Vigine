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
    /**
     * @brief Constructs an empty entity manager.
     *
     * Public because consumers of the modern @ref vigine::engine::IEngine
     * front door build their own @ref EntityManager alongside the
     * engine -- the modern @ref vigine::IContext aggregator carries
     * the @ref vigine::ecs::IECS wrapper instead, and no legacy
     * entity-manager handle is exposed through it. Examples and
     * downstream embedders construct one directly here, hand it to
     * each task that still walks the legacy @c Entity* surface, and
     * let it die alongside the surrounding scope.
     */
    EntityManager();
    ~EntityManager() override;
    Entity *createEntity();
    void removeEntity(Entity *entity);
    void addAlias(Entity *entity, const std::string &alias);
    Entity *getEntityByAlias(const std::string &alias) const;

  private:
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
};
} // namespace vigine
