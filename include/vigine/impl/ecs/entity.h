#pragma once

/**
 * @file entity.h
 * @brief Legacy concrete ECS entity identity.
 */

#include "vigine/api/ecs/abstractentity.h"

namespace vigine
{

/**
 * @brief Opaque identity for a single game-object in the legacy ECS.
 *
 * Components and systems reference an Entity by pointer; lifetime is
 * owned by EntityManager. The class derives from
 * @ref AbstractEntity so it satisfies the @ref IEntity contract; the
 * stable id reported by @ref IEntity::id is stamped by the entity
 * manager at construction time through the @ref Entity(IEntity::Id)
 * constructor.
 *
 * Strict encapsulation: no public data members. The id slot lives on
 * the @ref AbstractEntity base and the manager passes the value to
 * the constructor; once stamped, the id is immutable for the entity's
 * lifetime. The class has no default constructor by design -- every
 * legitimate Entity carries a non-zero id assigned at construction.
 */
class Entity final : public AbstractEntity
{
  public:
    /**
     * @brief Constructs an entity with the stable id @p id.
     *
     * Called by @ref EntityManager::createEntity, which is the only
     * sanctioned entry point. The manager passes a fresh non-zero id
     * from its monotonic counter; the value is forwarded to the
     * @ref AbstractEntity base via @ref AbstractEntity::setId.
     */
    explicit Entity(IEntity::Id id) noexcept { setId(id); }

    ~Entity() override = default;

    Entity(const Entity &)            = delete;
    Entity &operator=(const Entity &) = delete;
    Entity(Entity &&)                 = delete;
    Entity &operator=(Entity &&)      = delete;
};

} // namespace vigine
