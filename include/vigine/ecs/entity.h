#pragma once

/**
 * @file entity.h
 * @brief Legacy concrete ECS entity identity.
 */

namespace vigine
{

/**
 * @brief Opaque identity for a single game-object in the legacy ECS.
 *
 * Components and systems reference an Entity by pointer; lifetime is
 * owned by EntityManager. The class itself carries no state in the
 * public header -- it is a handle that engines extend internally.
 */
class Entity
{
  public:
    virtual ~Entity();
};

} // namespace vigine
