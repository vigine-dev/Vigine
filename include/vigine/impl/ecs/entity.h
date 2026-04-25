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
 * manager at construction time.
 *
 * Strict encapsulation: no public data members. The id slot lives on
 * the @ref AbstractEntity base and the manager passes the value to
 * the constructor; once stamped, the id is immutable for the entity's
 * lifetime.
 */
class Entity final : public AbstractEntity
{
  public:
    Entity() = default;

    /**
     * @brief Constructs an entity with the stable id @p id.
     *
     * Called by the entity manager. Callers outside the manager use
     * the default constructor and let the manager stamp the id.
     */
    explicit Entity(IEntity::Id id) noexcept { setId(id); }

    ~Entity() override = default;

    Entity(const Entity &)            = delete;
    Entity &operator=(const Entity &) = delete;
    Entity(Entity &&)                 = delete;
    Entity &operator=(Entity &&)      = delete;
};

} // namespace vigine
