#pragma once

#include <memory>
#include <vector>

#include "vigine/ecs/ecstypes.h"
#include "vigine/ecs/iecs.h"
#include "vigine/result.h"

namespace vigine::ecs
{
/**
 * @brief Thin read-write view over an @ref IECS that presents only
 *        the component-attachment surface.
 *
 * @ref IComponentStore mirrors the four component-lifecycle methods
 * on @ref IECS (@c attachComponent, @c detachComponent,
 * @c findComponent, @c componentsOf) without re-exposing the entity
 * lifecycle and bulk-query surface. Systems that only need to manage
 * their own component type on behalf of externally created entities
 * depend on this narrower contract instead of taking a full
 * @ref IECS reference, which keeps their dependency surface minimal
 * and makes unit testing easier — a mock @ref IComponentStore is
 * trivial to stand up.
 *
 * The store is a view, not a second storage layer: every concrete
 * @ref IComponentStore delegates to the underlying @ref IECS. The
 * split exists for the user, not for duplication.
 *
 * Ownership semantics mirror @ref IECS exactly: the store takes
 * unique ownership of the component on attach and releases it on
 * detach or entity removal.
 *
 * INV-1 compliance: no template parameters. INV-10 compliance: @c I
 * prefix on a pure-virtual interface. INV-11 compliance: the surface
 * exposes only ECS domain handles; substrate graph types stay hidden.
 */
class IComponentStore
{
  public:
    virtual ~IComponentStore() = default;

    /**
     * @brief Takes ownership of @p component and attaches it to
     *        @p entity.
     *
     * Delegates to @ref IECS::attachComponent. Returns a successful
     * @ref Result when the attach succeeds; any error reported by the
     * underlying ECS (stale entity, null pointer) surfaces unchanged.
     */
    [[nodiscard]] virtual Result
        add(EntityId entity, std::unique_ptr<IComponent> component) = 0;

    /**
     * @brief Returns a non-owning view of the first component on
     *        @p entity whose type id matches @p typeId, or @c nullptr
     *        when no such component exists.
     *
     * Delegates to @ref IECS::findComponent.
     */
    [[nodiscard]] virtual const IComponent *
        find(EntityId entity, ComponentTypeId typeId) const = 0;

    /**
     * @brief Releases the first component on @p entity whose type id
     *        matches @p typeId.
     *
     * Delegates to @ref IECS::detachComponent. Idempotent.
     */
    [[nodiscard]] virtual Result
        remove(EntityId entity, ComponentTypeId typeId) = 0;

    /**
     * @brief Returns non-owning views of every component currently
     *        attached to @p entity.
     *
     * Delegates to @ref IECS::componentsOf.
     */
    [[nodiscard]] virtual std::vector<const IComponent *>
        componentsOf(EntityId entity) const = 0;

    IComponentStore(const IComponentStore &)            = delete;
    IComponentStore &operator=(const IComponentStore &) = delete;
    IComponentStore(IComponentStore &&)                 = delete;
    IComponentStore &operator=(IComponentStore &&)      = delete;

  protected:
    IComponentStore() = default;
};

} // namespace vigine::ecs
