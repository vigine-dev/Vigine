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
 *   - a virtual @ref kind accessor returning a @ref ComponentKind
 *     (@c Unknown by default for components that have not yet been
 *     migrated onto the new contract).
 *
 * The legacy @ref AbstractComponent base in
 * @c include/vigine/api/ecs/abstractcomponent.h derives from this
 * interface so existing concrete components compile unchanged on day
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
     * The default implementation returns @c ComponentKind::Unknown so
     * legacy components that have not been migrated onto the new
     * contract still satisfy the interface; the @ref ComponentManager
     * rejects @c Unknown values from manager-side mutator paths so
     * unmigrated callers fail fast rather than silently routing into
     * a generic bucket.
     */
    [[nodiscard]] virtual ComponentKind kind() const noexcept
    {
        return ComponentKind::Unknown;
    }

    IComponent(const IComponent &)            = delete;
    IComponent &operator=(const IComponent &) = delete;
    IComponent(IComponent &&)                 = delete;
    IComponent &operator=(IComponent &&)      = delete;

  protected:
    IComponent() = default;
};

} // namespace vigine
