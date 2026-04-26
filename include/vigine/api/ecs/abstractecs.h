#pragma once

#include <memory>
#include <vector>

#include "vigine/api/ecs/ecstypes.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/result.h"

namespace vigine::ecs
{
// Forward declaration only. The concrete EntityWorld type is a
// substrate-primitive specialisation defined under @c src/ecs and is
// never exposed in the public header tree — see INV-11, wrapper
// encapsulation.
class EntityWorld;

/**
 * @brief Stateful abstract base that every concrete ECS derives from.
 *
 * @ref AbstractECS is level 4 of the wrapper recipe used by the
 * engine's Level-1 subsystem wrappers. It carries the state every
 * concrete ECS shares — a private handle to the internal entity
 * world — and supplies default implementations of every
 * @ref IECS lifecycle and query method so that a minimal concrete
 * ECS only needs to seal the inheritance chain. The internal entity
 * world specialises the graph substrate (@c vigine::core::graph::AbstractGraph)
 * and translates between @ref EntityId and the substrate's own
 * identifier types inside its implementation.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. The
 * base is abstract in the logical sense; its default constructor
 * wires up a fresh internal entity world so every concrete ECS has a
 * live substrate to delegate to.
 *
 * Composition, not inheritance:
 *   - @ref AbstractECS HAS-A private @c std::unique_ptr<EntityWorld>.
 *     It does @b not inherit from the substrate primitive at the
 *     wrapper level. The internal entity world is the only place
 *     where substrate primitives enter the ECS stack, and it lives
 *     strictly under @c src/ecs. This keeps the public header tree
 *     free of substrate types (INV-11) and makes the "an ECS IS NOT
 *     a substrate graph" relationship explicit.
 *
 * Strict encapsulation:
 *   - All data members are @c private. Derived ECS classes reach
 *     internal state through @c protected accessors; the single
 *     getter returns a reference to the entity world so concrete
 *     derivatives can extend the default implementation without
 *     re-exporting the substrate on their own public surface.
 *
 * Thread-safety: the base inherits the entity world's thread-safety
 * policy (reader-writer mutex on the substrate primitive). Callers
 * may query and mutate concurrently; each mutation takes the
 * exclusive lock while each query takes a shared lock. The wrapper
 * layer does not add further synchronisation — every ECS-side access
 * path funnels through the world.
 */
class AbstractECS : public IECS
{
  public:
    ~AbstractECS() override;

    // ------ IECS: entity lifecycle ------

    [[nodiscard]] EntityId createEntity() override;
    Result                 removeEntity(EntityId entity) override;
    [[nodiscard]] bool     hasEntity(EntityId entity) const noexcept override;

    // ------ IECS: component lifecycle ------

    [[nodiscard]] ComponentHandle
        attachComponent(EntityId entity, std::unique_ptr<IComponent> component) override;
    Result detachComponent(EntityId entity, ComponentTypeId typeId) override;
    [[nodiscard]] const IComponent *
        findComponent(EntityId entity, ComponentTypeId typeId) const override;
    [[nodiscard]] std::vector<const IComponent *>
        componentsOf(EntityId entity) const override;

    // ------ IECS: bulk query ------

    [[nodiscard]] std::vector<EntityId>
        entitiesWith(ComponentTypeId typeId) const override;

    AbstractECS(const AbstractECS &)            = delete;
    AbstractECS &operator=(const AbstractECS &) = delete;
    AbstractECS(AbstractECS &&)                 = delete;
    AbstractECS &operator=(AbstractECS &&)      = delete;

  protected:
    AbstractECS();

    /**
     * @brief Returns a mutable reference to the internal entity world.
     *
     * Exposed as @c protected so that follow-up concrete ECS classes
     * can add their own specialised cache or traversal on top of the
     * default implementation without re-exporting the substrate on
     * the public surface. The reference is stable for the lifetime of
     * the @ref AbstractECS instance.
     */
    [[nodiscard]] EntityWorld       &world() noexcept;
    [[nodiscard]] const EntityWorld &world() const noexcept;

  private:
    /**
     * @brief Owns the internal entity world.
     *
     * The world is a substrate-primitive specialisation defined
     * under @c src/ecs; forward-declaring it here keeps the substrate
     * out of the public header tree. Held through a
     * @c std::unique_ptr so the world's full definition does not
     * have to leak through this header.
     */
    std::unique_ptr<EntityWorld> _world;
};

} // namespace vigine::ecs
