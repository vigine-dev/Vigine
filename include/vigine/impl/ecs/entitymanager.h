#pragma once

/**
 * @file entitymanager.h
 * @brief Legacy concrete EntityManager that owns Entity lifetimes.
 */

#include "vigine/api/ecs/abstractentitymanager.h"
#include "vigine/api/ecs/ientity.h"
#include "vigine/impl/ecs/entity.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vigine
{
using EntityUPtr = std::unique_ptr<Entity>;

/**
 * @brief Owns the live set of Entity objects and their string aliases.
 *
 * @ref createEntity allocates a new Entity, stamps it with a fresh
 * monotonic @ref IEntity::Id, and keeps it alive until
 * @ref removeEntity releases it. Optional string aliases registered
 * via @ref addAlias allow lookup by @ref getEntityByAlias.
 *
 * Closes the legacy entity manager chain: derives from
 * @ref AbstractEntityManager, which in turn implements the pure
 * @ref IEntityManager contract. The interface signatures use
 * @ref IEntity *; the concrete narrows the return types to
 * @ref Entity * via covariant overrides so callers that already work
 * against the legacy substrate continue to receive @c Entity * without
 * an extra cast at the call site.
 */
class EntityManager final : public AbstractEntityManager
{
  public:
    /**
     * @brief Constructs an empty entity manager.
     *
     * The default-built manager is created by @ref AbstractContext
     * during context construction so every task observes a live
     * @ref IEntityManager through @c apiToken()->entityManager()
     * without anyone wiring it up explicitly. Embedders that need a
     * different concrete implementation register theirs through
     * @c IContext::setEntityManager and the default is destroyed via
     * the unique_ptr slot's RAII chain.
     */
    EntityManager();
    ~EntityManager() override;

    // ------ IEntityManager ------

    [[nodiscard]] Entity *createEntity() override;
    void                  removeEntity(IEntity *entity) override;
    void addAlias(IEntity *entity, const std::string &alias) override;
    [[nodiscard]] Entity *getEntityByAlias(const std::string &alias) const override;

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
