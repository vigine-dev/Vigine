#include "ecs/entityworld.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "vigine/ecs/ecstypes.h"
#include "vigine/ecs/iecs.h"
#include "vigine/ecs/kind.h"
#include "vigine/graph/abstractgraph.h"
#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/igraphquery.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"
#include "vigine/result.h"

namespace vigine::ecs
{

// ---------------------------------------------------------------------------
// Private helper nodes / edges. These types live entirely inside the
// translation unit — the wrapper's public header never mentions them, so
// they are free to refer to substrate types directly without breaching
// INV-11.
// ---------------------------------------------------------------------------

namespace
{

/**
 * @brief Entity vertex stored inside the specialised entity world.
 *
 * Carries only the @c vigine::ecs::kind::Entity tag and cooperates with
 * @c AbstractGraph::IdStamp so the assigned generational id flows back
 * to @c id() without a round-trip through the graph.
 */
class EntityNode final
    : public vigine::graph::INode
    , public vigine::graph::AbstractGraph::IdStamp
{
  public:
    EntityNode() = default;

    [[nodiscard]] vigine::graph::NodeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::graph::NodeKind kind() const noexcept override
    {
        return vigine::ecs::kind::Entity;
    }

    void onGraphIdAssigned(
        vigine::graph::NodeId nodeId,
        vigine::graph::EdgeId edgeId) noexcept override
    {
        _id = nodeId;
        (void)edgeId;
    }

  private:
    vigine::graph::NodeId _id{};
};

/**
 * @brief Component vertex that owns the attached @ref IComponent.
 *
 * The component is released together with the node when the entity
 * world removes the slot, so every attached component lives for
 * exactly as long as the graph tracks it.
 */
class ComponentNode final
    : public vigine::graph::INode
    , public vigine::graph::AbstractGraph::IdStamp
{
  public:
    explicit ComponentNode(std::unique_ptr<IComponent> component) noexcept
        : _component{std::move(component)}
    {
    }

    [[nodiscard]] vigine::graph::NodeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::graph::NodeKind kind() const noexcept override
    {
        return vigine::ecs::kind::Component;
    }

    /**
     * @brief Returns a non-owning reference to the wrapped component.
     *
     * Never returns @c nullptr on a live node: the attach path on
     * @ref EntityWorld rejects a null component before construction.
     */
    [[nodiscard]] const IComponent *component() const noexcept { return _component.get(); }

    void onGraphIdAssigned(
        vigine::graph::NodeId nodeId,
        vigine::graph::EdgeId edgeId) noexcept override
    {
        _id = nodeId;
        (void)edgeId;
    }

  private:
    vigine::graph::NodeId       _id{};
    std::unique_ptr<IComponent> _component;
};

/**
 * @brief Directed edge that ties a component vertex back to its
 *        owning entity.
 *
 * Carries no payload — the attachment itself is the semantic; the
 * component node on the @c to() end keeps the component pointer.
 */
class AttachedEdge final
    : public vigine::graph::IEdge
    , public vigine::graph::AbstractGraph::IdStamp
{
  public:
    AttachedEdge(vigine::graph::NodeId from, vigine::graph::NodeId to) noexcept
        : _from{from}, _to{to}
    {
    }

    [[nodiscard]] vigine::graph::EdgeId   id() const noexcept override { return _id; }
    [[nodiscard]] vigine::graph::EdgeKind kind() const noexcept override
    {
        return vigine::ecs::edge_kind::Attached;
    }
    [[nodiscard]] vigine::graph::NodeId from() const noexcept override { return _from; }
    [[nodiscard]] vigine::graph::NodeId to() const noexcept override { return _to; }
    [[nodiscard]] const vigine::graph::IEdgeData *data() const noexcept override
    {
        return nullptr;
    }

    void onGraphIdAssigned(
        vigine::graph::NodeId nodeId,
        vigine::graph::EdgeId edgeId) noexcept override
    {
        _id = edgeId;
        (void)nodeId;
    }

  private:
    vigine::graph::EdgeId _id{};
    vigine::graph::NodeId _from{};
    vigine::graph::NodeId _to{};
};

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

EntityWorld::EntityWorld() = default;

EntityWorld::~EntityWorld() = default;

// ---------------------------------------------------------------------------
// POD translation helpers. The @ref EntityId / @ref ComponentHandle
// layouts are intentionally identical to @c vigine::graph::NodeId so the
// translation is a plain field-for-field copy; INV-11 allows it here
// because the conversion lives entirely inside the wrapper
// implementation.
// ---------------------------------------------------------------------------

vigine::graph::NodeId EntityWorld::toNodeId(EntityId entity) noexcept
{
    return vigine::graph::NodeId{entity.index, entity.generation};
}

vigine::graph::NodeId EntityWorld::toNodeId(ComponentHandle handle) noexcept
{
    return vigine::graph::NodeId{handle.index, handle.generation};
}

EntityId EntityWorld::toEntityId(vigine::graph::NodeId node) noexcept
{
    return EntityId{node.index, node.generation};
}

ComponentHandle EntityWorld::toComponentHandle(vigine::graph::NodeId node) noexcept
{
    return ComponentHandle{node.index, node.generation};
}

// ---------------------------------------------------------------------------
// Entity lifecycle.
// ---------------------------------------------------------------------------

EntityId EntityWorld::createEntity()
{
    // The base graph never returns an invalid generation from addNode, so
    // the fresh @ref EntityId is always valid. Passing the node back as an
    // @c INode unique_ptr follows the IdStamp handshake — the node
    // captures the assigned id during insert.
    auto                        node = std::make_unique<EntityNode>();
    const vigine::graph::NodeId nid  = addNode(std::move(node));

    {
        std::unique_lock lock(_entitiesMutex);
        _entities.push_back(nid);
    }

    return toEntityId(nid);
}

Result EntityWorld::removeEntity(EntityId entity)
{
    if (!entity.valid())
    {
        return Result(Result::Code::Error, "invalid entity id");
    }

    const auto nid = toNodeId(entity);
    if (!query().hasNode(nid))
    {
        return Result(Result::Code::Error, "stale entity id");
    }

    // Cascade component removal first so the graph layer only has to
    // clean up the entity node at the end. The underlying graph supports
    // cascading removeNode internally; doing the walk here keeps the
    // component ownership release in deterministic LIFO order.
    const auto attachedEdges
        = query().outEdgesOfKind(nid, vigine::ecs::edge_kind::Attached);
    for (auto it = attachedEdges.rbegin(); it != attachedEdges.rend(); ++it)
    {
        const vigine::graph::IEdge *e = edge(*it);
        if (e == nullptr)
        {
            continue;
        }
        const vigine::graph::NodeId componentNode = e->to();
        // removeNode cascades the edge for us; calling it first on the
        // component makes ownership release order deterministic.
        (void)removeNode(componentNode);
    }

    Result graphResult = removeNode(nid);
    if (!graphResult.isSuccess())
    {
        return graphResult;
    }

    // Erase the matching slot from the live-entity side-table. Multiple
    // recycled slots with different generations can share an index; only
    // the exact (index, generation) match is a dead duplicate of the
    // caller's handle.
    {
        std::unique_lock lock(_entitiesMutex);
        _entities.erase(
            std::remove(_entities.begin(), _entities.end(), nid),
            _entities.end());
    }

    return graphResult;
}

bool EntityWorld::hasEntity(EntityId entity) const noexcept
{
    if (!entity.valid())
    {
        return false;
    }
    const vigine::graph::INode *n = node(toNodeId(entity));
    return n != nullptr && n->kind() == vigine::ecs::kind::Entity;
}

// ---------------------------------------------------------------------------
// Component lifecycle.
// ---------------------------------------------------------------------------

ComponentHandle EntityWorld::attachComponent(
    EntityId entity, std::unique_ptr<IComponent> component)
{
    if (!entity.valid() || component == nullptr)
    {
        return ComponentHandle{};
    }

    const vigine::graph::NodeId entityNode = toNodeId(entity);
    if (!query().hasNode(entityNode))
    {
        return ComponentHandle{};
    }

    auto                        compNode = std::make_unique<ComponentNode>(std::move(component));
    const vigine::graph::NodeId compId   = addNode(std::move(compNode));
    if (!compId.valid())
    {
        return ComponentHandle{};
    }

    auto                        attachment = std::make_unique<AttachedEdge>(entityNode, compId);
    const vigine::graph::EdgeId eid        = addEdge(std::move(attachment));
    if (!eid.valid())
    {
        // Adding the edge failed — roll the component node back so the
        // graph does not keep a dangling component with no entity link.
        (void)removeNode(compId);
        return ComponentHandle{};
    }

    return toComponentHandle(compId);
}

Result EntityWorld::detachComponent(EntityId entity, ComponentTypeId typeId)
{
    if (!entity.valid())
    {
        return Result(Result::Code::Error, "invalid entity id");
    }

    const vigine::graph::NodeId entityNode = toNodeId(entity);
    if (!query().hasNode(entityNode))
    {
        return Result(Result::Code::Error, "stale entity id");
    }

    const auto attachedEdges
        = query().outEdgesOfKind(entityNode, vigine::ecs::edge_kind::Attached);
    for (vigine::graph::EdgeId eid : attachedEdges)
    {
        const vigine::graph::IEdge *e = edge(eid);
        if (e == nullptr)
        {
            continue;
        }
        const vigine::graph::INode *target = node(e->to());
        const auto                 *cn     = dynamic_cast<const ComponentNode *>(target);
        if (cn == nullptr || cn->component() == nullptr)
        {
            continue;
        }
        if (cn->component()->componentTypeId() != typeId)
        {
            continue;
        }
        // removeNode cascades the @c Attached edge automatically.
        return removeNode(e->to());
    }
    return Result(Result::Code::Error, "component not attached");
}

const IComponent *EntityWorld::findComponent(
    EntityId entity, ComponentTypeId typeId) const
{
    if (!entity.valid())
    {
        return nullptr;
    }

    const vigine::graph::NodeId entityNode = toNodeId(entity);
    if (!query().hasNode(entityNode))
    {
        return nullptr;
    }

    const auto attachedEdges
        = query().outEdgesOfKind(entityNode, vigine::ecs::edge_kind::Attached);
    for (vigine::graph::EdgeId eid : attachedEdges)
    {
        const vigine::graph::IEdge *e = edge(eid);
        if (e == nullptr)
        {
            continue;
        }
        const vigine::graph::INode *target = node(e->to());
        const auto                 *cn     = dynamic_cast<const ComponentNode *>(target);
        if (cn == nullptr || cn->component() == nullptr)
        {
            continue;
        }
        if (cn->component()->componentTypeId() == typeId)
        {
            return cn->component();
        }
    }
    return nullptr;
}

std::vector<const IComponent *> EntityWorld::componentsOf(EntityId entity) const
{
    std::vector<const IComponent *> out;
    if (!entity.valid())
    {
        return out;
    }

    const vigine::graph::NodeId entityNode = toNodeId(entity);
    if (!query().hasNode(entityNode))
    {
        return out;
    }

    const auto attachedEdges
        = query().outEdgesOfKind(entityNode, vigine::ecs::edge_kind::Attached);
    out.reserve(attachedEdges.size());
    for (vigine::graph::EdgeId eid : attachedEdges)
    {
        const vigine::graph::IEdge *e = edge(eid);
        if (e == nullptr)
        {
            continue;
        }
        const vigine::graph::INode *target = node(e->to());
        const auto                 *cn     = dynamic_cast<const ComponentNode *>(target);
        if (cn == nullptr || cn->component() == nullptr)
        {
            continue;
        }
        out.push_back(cn->component());
    }
    return out;
}

// ---------------------------------------------------------------------------
// Bulk query.
// ---------------------------------------------------------------------------

std::vector<EntityId> EntityWorld::entitiesWith(ComponentTypeId typeId) const
{
    // Snapshot the live-entity list under the shared lock, then walk
    // each entity's outgoing @c Attached edges without holding the
    // local mutex. The graph's own shared_mutex guards every per-edge
    // lookup, so concurrent attaches land under the graph's exclusive
    // lock and finish before this walk observes them.
    std::vector<vigine::graph::NodeId> snapshot;
    {
        std::shared_lock lock(_entitiesMutex);
        snapshot = _entities;
    }

    std::vector<EntityId> out;
    out.reserve(snapshot.size());
    for (vigine::graph::NodeId entityNode : snapshot)
    {
        if (!query().hasNode(entityNode))
        {
            continue;
        }
        const auto attachedEdges
            = query().outEdgesOfKind(entityNode, vigine::ecs::edge_kind::Attached);
        bool matched = false;
        for (vigine::graph::EdgeId eid : attachedEdges)
        {
            const vigine::graph::IEdge *e = edge(eid);
            if (e == nullptr)
            {
                continue;
            }
            const vigine::graph::INode *target = node(e->to());
            const auto                 *cn     = dynamic_cast<const ComponentNode *>(target);
            if (cn == nullptr || cn->component() == nullptr)
            {
                continue;
            }
            if (cn->component()->componentTypeId() == typeId)
            {
                matched = true;
                break;
            }
        }
        if (matched)
        {
            out.push_back(toEntityId(entityNode));
        }
    }
    return out;
}

} // namespace vigine::ecs
