#pragma once

#include <memory>
#include <vector>

#include "vigine/ecs/ecstypes.h"
#include "vigine/result.h"

namespace vigine::ecs
{
/**
 * @brief Pure-virtual root of every component type attached to an
 *        entity through an @ref IECS.
 *
 * A component is a piece of state owned by a specific entity; the
 * engine attaches, enumerates, and detaches components through the
 * @ref IECS surface. Concrete components carry their domain payload
 * (render mesh, window handle, postgres connection parameters, ...)
 * and report a stable @ref ComponentTypeId so the receiver can
 * downcast safely without @c dynamic_cast.
 *
 * Ownership: the ECS takes unique ownership of the component when it
 * is attached (@ref IECS::attachComponent). The component stays live
 * until the matching detach or until the owning entity is removed.
 *
 * Copy and move are deleted to keep the pinning invariant explicit:
 * once attached, a component never migrates between entities.
 *
 * INV-1 compliance: no template parameters on the interface. Concrete
 * component types override @ref componentTypeId with a constant from
 * the engine or user-space range.
 */
class IComponent
{
  public:
    virtual ~IComponent() = default;

    /**
     * @brief Reports the engine-wide classifier of this component
     *        type.
     *
     * Constant for the lifetime of a concrete implementation; the
     * value is typically defined as a @c constexpr member on the
     * concrete class. Receivers compare the id against the expected
     * value before casting down.
     */
    [[nodiscard]] virtual ComponentTypeId componentTypeId() const noexcept = 0;

    IComponent(const IComponent &)            = delete;
    IComponent &operator=(const IComponent &) = delete;
    IComponent(IComponent &&)                 = delete;
    IComponent &operator=(IComponent &&)      = delete;

  protected:
    IComponent() = default;
};

/**
 * @brief Pure-virtual Level-1 wrapper surface for the entity component
 *        system.
 *
 * @ref IECS is the user-facing contract over the ECS substrate: it
 * creates and removes entities, attaches and detaches components, and
 * surfaces bulk queries over component types. The interface knows
 * nothing about the underlying graph storage; substrate primitive
 * types never appear in the public API per INV-11. The stateful base
 * @ref AbstractECS carries an opaque internal entity world through a
 * private @c std::unique_ptr so the substrate stays hidden from
 * consumers of this header.
 *
 * Ownership and lifetime:
 *   - Concrete ECS instances are constructed through the non-template
 *     factory in @ref factory.h and handed back as
 *     @c std::unique_ptr<IECS>. The caller owns the returned pointer.
 *   - Entities are value handles (@ref EntityId); removing an entity
 *     cascades removal of every attached component automatically.
 *   - Components are owned by the ECS after @ref attachComponent
 *     succeeds. Detach or entity removal both release ownership.
 *
 * Thread-safety: the contract does not fix one. The default
 * implementation inherits the substrate's reader-writer policy; the
 * concrete ECS exposed through @c createECS serialises mutations with
 * the same @c std::shared_mutex the underlying graph uses. Concurrent
 * queries are safe with each other; concurrent mutations take the
 * exclusive lock.
 *
 * INV-1 compliance: the surface uses no template parameters. INV-10
 * compliance: the name carries the @c I prefix for a pure-virtual
 * interface. INV-11 compliance: the public API mentions only ECS
 * domain handles (@ref EntityId, @ref ComponentHandle,
 * @ref ComponentTypeId, @ref IComponent); no graph primitive types
 * cross the boundary.
 */
class IECS
{
  public:
    virtual ~IECS() = default;

    // ------ Entity lifecycle ------

    /**
     * @brief Allocates a fresh entity slot and returns its generational
     *        handle.
     *
     * The returned handle is always valid; implementations never
     * report a generation of zero from @ref createEntity. Callers pass
     * the handle to @ref attachComponent, @ref detachComponent,
     * @ref removeEntity, and the query methods.
     */
    [[nodiscard]] virtual EntityId createEntity() = 0;

    /**
     * @brief Removes the entity addressed by @p entity along with
     *        every attached component.
     *
     * Idempotent: removing a stale handle reports a
     * @ref Result::Code::Error status without side effects. On success
     * every previously attached component is released in the reverse
     * order it was attached; holders of dangling @ref ComponentHandle
     * values observe them as invalid on the next lookup.
     */
    virtual Result removeEntity(EntityId entity) = 0;

    /**
     * @brief Reports whether the ECS currently tracks the entity
     *        addressed by @p entity.
     *
     * Useful for pre-flight checks in systems that want to skip
     * silently rather than error out when an entity has been removed
     * by another path between ticks.
     */
    [[nodiscard]] virtual bool hasEntity(EntityId entity) const noexcept = 0;

    // ------ Component lifecycle ------

    /**
     * @brief Takes ownership of @p component and attaches it to the
     *        entity addressed by @p entity.
     *
     * Returns a fresh generational @ref ComponentHandle that addresses
     * the new attachment. Passing a null pointer or a stale entity
     * handle is a programming error; the implementation reports an
     * invalid handle in that case.
     */
    [[nodiscard]] virtual ComponentHandle
        attachComponent(EntityId entity, std::unique_ptr<IComponent> component) = 0;

    /**
     * @brief Releases the first component attached to @p entity whose
     *        @ref IComponent::componentTypeId matches @p typeId.
     *
     * Returns a successful @ref Result when a matching component was
     * detached; reports @ref Result::Code::Error when no attached
     * component carries the requested type. Implementations are
     * idempotent: subsequent calls with the same parameters after the
     * first detach return an error without side effects.
     */
    virtual Result detachComponent(EntityId entity, ComponentTypeId typeId) = 0;

    /**
     * @brief Returns a non-owning view of the first component on
     *        @p entity whose type id matches @p typeId, or @c nullptr
     *        when no such component exists.
     *
     * The returned pointer is valid until the next ECS mutation that
     * touches @p entity; callers that need longer-lived references
     * should keep the @ref ComponentHandle and re-resolve through the
     * ECS.
     */
    [[nodiscard]] virtual const IComponent *
        findComponent(EntityId entity, ComponentTypeId typeId) const = 0;

    /**
     * @brief Returns non-owning views of every component currently
     *        attached to @p entity.
     *
     * Returns an empty vector when @p entity is stale or carries no
     * components. The order matches attach order; equal-type
     * components keep their relative attach order.
     */
    [[nodiscard]] virtual std::vector<const IComponent *>
        componentsOf(EntityId entity) const = 0;

    // ------ Bulk query ------

    /**
     * @brief Returns every entity that currently carries at least one
     *        component whose type id matches @p typeId.
     *
     * The vector is a snapshot; subsequent mutations do not invalidate
     * it. Callers iterate freely without holding any ECS lock.
     */
    [[nodiscard]] virtual std::vector<EntityId>
        entitiesWith(ComponentTypeId typeId) const = 0;

    IECS(const IECS &)            = delete;
    IECS &operator=(const IECS &) = delete;
    IECS(IECS &&)                 = delete;
    IECS &operator=(IECS &&)      = delete;

  protected:
    IECS() = default;
};

} // namespace vigine::ecs
