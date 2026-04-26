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
 * concrete components share. The base ships the
 * @c ComponentKind::Unknown default body for @ref IComponent::kind so
 * legacy concretes that have not been migrated onto the new contract
 * still compile through the abstract base; subclasses opt into a
 * meaningful kind by overriding only the accessor they care about.
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

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::Unknown;
    }

    AbstractComponent(const AbstractComponent &)            = delete;
    AbstractComponent &operator=(const AbstractComponent &) = delete;
    AbstractComponent(AbstractComponent &&)                 = delete;
    AbstractComponent &operator=(AbstractComponent &&)      = delete;

  protected:
    AbstractComponent() = default;
};

} // namespace vigine
