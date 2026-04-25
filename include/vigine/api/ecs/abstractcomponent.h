#pragma once

/**
 * @file abstractcomponent.h
 * @brief Legacy abstract base class for ECS components.
 */

#include "vigine/api/ecs/icomponent.h"

namespace vigine
{

/**
 * @brief Stateful abstract base shared by every legacy ECS component.
 *
 * @ref AbstractComponent is the level-4 base in the wrapper recipe
 * applied to the legacy component tree: it derives from the pure
 * @ref IComponent contract and carries the (currently empty) state
 * concrete components share. The base ships @ref kind from
 * @ref IComponent unchanged so concrete components opt into a non
 * @c Unknown kind by overriding only the accessor they care about.
 *
 * The base carries no data members today; the door is open for the
 * follow-up leaf to fold shared component state in (e.g. a back-pointer
 * to the owning entity) without breaking source compatibility for
 * existing concrete components.
 */
class AbstractComponent : public IComponent
{
  public:
    ~AbstractComponent() override = default;

    AbstractComponent(const AbstractComponent &)            = delete;
    AbstractComponent &operator=(const AbstractComponent &) = delete;
    AbstractComponent(AbstractComponent &&)                 = delete;
    AbstractComponent &operator=(AbstractComponent &&)      = delete;

  protected:
    AbstractComponent() = default;
};

} // namespace vigine
