#pragma once

/**
 * @file abstractentity.h
 * @brief Legacy root base class for ECS entities.
 */

#include "abstractcomponent.h"

#include <string>
#include <vector>

namespace vigine
{
/**
 * @brief Empty polymorphic base for legacy ECS entity classes.
 *
 * Acts as the common type for entity containers and references.
 * Carries no state; the concrete Entity class in entity.h fills in
 * lifecycle details.
 */
class AbstractEntity
{
  public:
    virtual ~AbstractEntity() {};

  protected:
    AbstractEntity() = default;
};

} // namespace vigine
