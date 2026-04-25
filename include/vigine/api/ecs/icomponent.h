#pragma once

#include "vigine/api/ecs/componentkind.h"

namespace vigine
{
/**
 * @brief Pure-virtual root interface for legacy components stored by
 *        @ref ComponentManager.
 *
 * @ref IComponent replaces the per-type-template path the legacy
 * @ref ComponentManager carried before this leaf with a non-template
 * virtual contract. Concrete components report a stable
 * @ref ComponentKind so a non-template manager can route attach /
 * detach / lookup calls without ever instantiating a template on the
 * caller's behalf.
 *
 * The contract is deliberately tiny:
 *   - a virtual destructor so the manager can hold owning
 *     @c std::unique_ptr<IComponent>,
 *   - a pure-virtual @ref kind accessor returning a @ref ComponentKind
 *     classifier the manager keys storage on.
 *
 * The legacy @ref AbstractComponent base in
 * @c include/vigine/api/ecs/abstractcomponent.h derives from this
 * interface and ships the @c ComponentKind::Unknown default body for
 * @ref kind so existing concrete components compile unchanged on day
 * one; subclasses opt into the new contract by overriding @ref kind.
 *
 * INV-1 compliance: no template parameters anywhere on the surface.
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies.
 */
class IComponent
{
  public:
    virtual ~IComponent() = default;

    /**
     * @brief Reports the @ref ComponentKind classifier for this
     *        concrete component.
     *
     * The interface keeps this method pure (per INV-10); the default
     * @c ComponentKind::Unknown body lives on @ref AbstractComponent
     * so legacy components that have not been migrated onto the new
     * contract still satisfy the interface through the abstract base.
     * The @ref ComponentManager rejects @c Unknown values from
     * manager-side mutator paths so unmigrated callers fail fast
     * rather than silently routing into a generic bucket.
     */
    [[nodiscard]] virtual ComponentKind kind() const noexcept = 0;

    IComponent(const IComponent &)            = delete;
    IComponent &operator=(const IComponent &) = delete;
    IComponent(IComponent &&)                 = delete;
    IComponent &operator=(IComponent &&)      = delete;

  protected:
    IComponent() = default;
};

} // namespace vigine
