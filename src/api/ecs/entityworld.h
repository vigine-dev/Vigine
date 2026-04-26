#pragma once

#include <memory>
#include <shared_mutex>
#include <vector>

#include "vigine/api/ecs/ecstypes.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/core/graph/abstractgraph.h"
#include "vigine/core/graph/nodeid.h"
#include "vigine/result.h"

namespace vigine::ecs
{
/**
 * @brief Internal graph specialisation that the ECS wrapper uses to
 *        hold its entity-and-component storage.
 *
 * @ref EntityWorld is a concrete @c vigine::core::graph::AbstractGraph
 * subtype that seals the inheritance chain for the ECS wrapper. It
 * carries the translation between the ECS's own POD handles
 * (@ref EntityId, @ref ComponentHandle) and the substrate's
 * generational @c NodeId so every @ref IECS method can delegate a
 * single call to it without letting substrate primitives leak through
 * the public wrapper surface.
 *
 * This header lives under @c src/ecs on purpose: the INV-11 rule
 * forbids @c vigine::core::graph types from surfacing in
 * @c include/vigine/ecs. Only the wrapper implementation consumes the
 * world; callers of @ref IECS / @ref AbstractECS see neither the
 * world nor its graph base.
 *
 * Thread-safety inherits from @c AbstractGraph: every mutating entry
 * point takes the graph's exclusive lock; reads take a shared lock.
 * The wrapper layer does not add any additional synchronisation on
 * top; every ECS-side access path funnels through the world.
 *
 * Node kinds used:
 *   - @c vigine::ecs::kind::Entity for entity nodes.
 *   - @c vigine::ecs::kind::Component for component nodes.
 *
 * Edge kinds used:
 *   - @c vigine::ecs::edge_kind::Attached for the directed edge that
 *     ties a component node back to its owning entity.
 */
class EntityWorld final : public vigine::core::graph::AbstractGraph
{
  public:
    EntityWorld();
    ~EntityWorld() override;

    EntityWorld(const EntityWorld &)            = delete;
    EntityWorld &operator=(const EntityWorld &) = delete;
    EntityWorld(EntityWorld &&)                 = delete;
    EntityWorld &operator=(EntityWorld &&)      = delete;

    // Mutex protecting the private @c _entities list below. The
    // underlying @c AbstractGraph already serialises access to its own
    // storage with a reader-writer mutex; this second mutex is here
    // because the wrapper keeps a small side-table of live entity ids
    // so @ref entitiesWith can enumerate without probing the graph via
    // fragile index math.

    /**
     * @brief Allocates a fresh entity node and returns the
     *        corresponding @ref EntityId.
     *
     * The returned handle is always valid; the underlying graph never
     * reports a generation of zero from @ref AbstractGraph::addNode.
     */
    [[nodiscard]] EntityId createEntity();

    /**
     * @brief Removes the entity node addressed by @p entity along
     *        with every attached component node and its attachment
     *        edge.
     *
     * Delegates to @ref AbstractGraph::removeNode for the entity
     * itself; attached components are released first so the graph
     * layer only has to clean up the entity slot. Idempotent: a stale
     * handle reports an @ref Result::Code::Error status without
     * side effects.
     */
    Result removeEntity(EntityId entity);

    /**
     * @brief Reports whether an entity node addressed by @p entity is
     *        currently tracked.
     */
    [[nodiscard]] bool hasEntity(EntityId entity) const noexcept;

    /**
     * @brief Creates a component node wrapping @p component and
     *        attaches it to the entity addressed by @p entity through
     *        a directed @c Attached edge.
     *
     * Returns the fresh @ref ComponentHandle addressing the new
     * component node. Returns a default-constructed (invalid) handle
     * when @p component is null or @p entity is stale.
     */
    [[nodiscard]] ComponentHandle
        attachComponent(EntityId entity, std::unique_ptr<IComponent> component);

    /**
     * @brief Releases the first component attached to @p entity whose
     *        @ref IComponent::componentTypeId equals @p typeId.
     *
     * Removes both the component node and the @c Attached edge that
     * tied it to the entity. Reports @ref Result::Code::Error when
     * no matching component exists.
     */
    Result detachComponent(EntityId entity, ComponentTypeId typeId);

    /**
     * @brief Returns a non-owning view of the first component on
     *        @p entity whose type id matches @p typeId, or @c nullptr
     *        when no such component exists.
     */
    [[nodiscard]] const IComponent *
        findComponent(EntityId entity, ComponentTypeId typeId) const;

    /**
     * @brief Returns non-owning views of every component currently
     *        attached to @p entity.
     */
    [[nodiscard]] std::vector<const IComponent *>
        componentsOf(EntityId entity) const;

    /**
     * @brief Returns every entity that currently carries at least one
     *        component whose type id matches @p typeId.
     */
    [[nodiscard]] std::vector<EntityId>
        entitiesWith(ComponentTypeId typeId) const;

    /**
     * @brief Translates an @ref EntityId to the substrate's
     *        @c NodeId.
     *
     * Packaged as a free-standing @c static helper so the wrapper
     * implementation can reach the substrate without needing further
     * access to the graph's internals. The two POD types have the
     * same layout; the helper exists for type-safety, not for
     * arithmetic.
     */
    [[nodiscard]] static vigine::core::graph::NodeId toNodeId(EntityId entity) noexcept;

    /**
     * @brief Translates an @ref ComponentHandle to the substrate's
     *        @c NodeId.
     */
    [[nodiscard]] static vigine::core::graph::NodeId toNodeId(ComponentHandle handle) noexcept;

    /**
     * @brief Translates a substrate @c NodeId back to an
     *        @ref EntityId.
     *
     * Only the wrapper implementation calls this; callers of the
     * public ECS API never see the substrate type.
     */
    [[nodiscard]] static EntityId toEntityId(vigine::core::graph::NodeId node) noexcept;

    /**
     * @brief Translates a substrate @c NodeId back to a
     *        @ref ComponentHandle.
     */
    [[nodiscard]] static ComponentHandle
        toComponentHandle(vigine::core::graph::NodeId node) noexcept;

  private:
    /**
     * @brief Serialises access to the live-entity side-table below.
     *
     * The underlying @c AbstractGraph already locks its own storage;
     * this mutex lives alongside so that the two pieces of state the
     * wrapper mutates in concert (the graph slot and the entity id
     * list) stay consistent with each other. Mutations take the
     * exclusive lock; read-only scans (e.g. @ref entitiesWith) take
     * the shared lock.
     */
    mutable std::shared_mutex _entitiesMutex;

    /**
     * @brief Live entity ids in creation order.
     *
     * Kept alongside the graph so that @ref entitiesWith can enumerate
     * without probing the substrate via fragile index math. Mutated
     * only by @ref createEntity (push) and @ref removeEntity (erase).
     */
    std::vector<vigine::core::graph::NodeId> _entities;
};

} // namespace vigine::ecs
