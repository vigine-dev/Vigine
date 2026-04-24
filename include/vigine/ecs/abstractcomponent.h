#pragma once

/**
 * @file abstractcomponent.h
 * @brief Legacy root base class for ECS components.
 */

namespace vigine
{

/**
 * @brief Empty polymorphic base shared by every legacy ECS component.
 *
 * Provides a common pointer type so ComponentManager can store
 * heterogeneous components through a single base. Carries no state
 * and no virtual methods other than the destructor.
 */
class AbstractComponent
{
  public:
    virtual ~AbstractComponent() = 0;

  protected:
    AbstractComponent() = default;
};

} // namespace vigine
