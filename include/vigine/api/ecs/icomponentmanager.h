#pragma once

#include <memory>
#include <vector>

#include "vigine/api/ecs/ecstypes.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/result.h"

namespace vigine::ecs
{
/**
 * @brief Thin read-write view over an @ref IECS that presents only
 *        the component-attachment surface.
 *
 * @ref IComponentManager mirrors the four component-lifecycle methods
 * on @ref IECS (@c attachComponent, @c detachComponent,
 * @c findComponent, @c componentsOf) without re-exposing the entity
 * lifecycle and bulk-query surface. Systems that only need to manage
 * their own component type on behalf of externally created entities
 * depend on this narrower contract instead of taking a full
 * @ref IECS reference, which keeps their dependency surface minimal
 * and makes unit testing easier — a mock @ref IComponentManager is
 * trivial to stand up.
 *
 * The manager is a view, not a second storage layer: every concrete
 * @ref IComponentManager delegates to the underlying @ref IECS. The
 * split exists for the user, not for duplication.
 *
 * Ownership semantics mirror @ref IECS exactly: the manager takes
 * unique ownership of the component on attach and releases it on
 * detach or entity removal.
 *
 * Naming: the file ships under @c vigine/api/ecs/icomponentmanager.h
 * after the @c icomponentstore.h rename so the wrapper-layer view
 * type aligns with the manager terminology used by the rest of the
 * Level-1 ECS stack (entity manager, component manager, system).
 *
 * INV-1 compliance: no template parameters. INV-10 compliance: @c I
 * prefix on a pure-virtual interface. INV-11 compliance: the surface
 * exposes only ECS domain handles; substrate graph types stay hidden.
 */
class IComponentManager
{
  public:
    virtual ~IComponentManager() = default;

    /**
     * @brief Takes ownership of @p component and attaches it to
     *        @p entity.
     *
     * Delegates to @ref IECS::attachComponent. That lower-level call
     * returns a @ref ComponentHandle and signals failure through an
     * invalid handle (no `Result` there). This surface translates the
     * outcome into a @ref Result: valid handle → Success; invalid
     * handle (stale entity, null pointer) → `Result::Code::Error`
     * with a short message naming the attach-failure cause. Systems
     * that need the @ref ComponentHandle itself should call
     * @ref IECS::attachComponent directly.
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

    IComponentManager(const IComponentManager &)            = delete;
    IComponentManager &operator=(const IComponentManager &) = delete;
    IComponentManager(IComponentManager &&)                 = delete;
    IComponentManager &operator=(IComponentManager &&)      = delete;

  protected:
    IComponentManager() = default;
};

} // namespace vigine::ecs
