#include "statemachine/statetopology.h"

#include <cstddef>
#include <memory>

#include "vigine/fsm/kind.h"
#include "vigine/core/graph/abstractgraph.h"
#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/iedge.h"
#include "vigine/core/graph/igraphquery.h"
#include "vigine/core/graph/inode.h"
#include "vigine/core/graph/kind.h"
#include "vigine/core/graph/nodeid.h"
#include "vigine/result.h"
#include "vigine/api/statemachine/stateid.h"

namespace vigine::statemachine
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
 * @brief State vertex stored inside the specialised state topology.
 *
 * Carries only the @c vigine::fsm::kind::State tag and cooperates
 * with @c AbstractGraph::IdStamp so the assigned generational id
 * flows back to @c id() without a round-trip through the graph.
 */
class StateNode final
    : public vigine::core::graph::INode
    , public vigine::core::graph::AbstractGraph::IdStamp
{
  public:
    StateNode() = default;

    [[nodiscard]] vigine::core::graph::NodeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::core::graph::NodeKind kind() const noexcept override
    {
        return vigine::fsm::kind::State;
    }

    void onGraphIdAssigned(
        vigine::core::graph::NodeId nodeId,
        vigine::core::graph::EdgeId edgeId) noexcept override
    {
        _id = nodeId;
        (void)edgeId;
    }

  private:
    vigine::core::graph::NodeId _id{};
};

/**
 * @brief Directed @c ChildOf edge from a child state to its parent.
 *
 * Carries no payload — the hierarchy itself is the semantic. The
 * direction is child->parent so the natural query at dispatch time
 * (walk the parent chain from the active state) matches
 * @c outEdgesOfKind on the child vertex.
 */
class ChildOfEdge final
    : public vigine::core::graph::IEdge
    , public vigine::core::graph::AbstractGraph::IdStamp
{
  public:
    ChildOfEdge(vigine::core::graph::NodeId from, vigine::core::graph::NodeId to) noexcept
        : _from{from}, _to{to}
    {
    }

    [[nodiscard]] vigine::core::graph::EdgeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::core::graph::EdgeKind kind() const noexcept override
    {
        return vigine::fsm::edge_kind::ChildOf;
    }
    [[nodiscard]] vigine::core::graph::NodeId from() const noexcept override { return _from; }
    [[nodiscard]] vigine::core::graph::NodeId to() const noexcept override { return _to; }
    [[nodiscard]] const vigine::core::graph::IEdgeData *data() const noexcept override
    {
        return nullptr;
    }

    void onGraphIdAssigned(
        vigine::core::graph::NodeId nodeId,
        vigine::core::graph::EdgeId edgeId) noexcept override
    {
        _id = edgeId;
        (void)nodeId;
    }

  private:
    vigine::core::graph::EdgeId _id{};
    vigine::core::graph::NodeId _from{};
    vigine::core::graph::NodeId _to{};
};

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

StateTopology::StateTopology() = default;

StateTopology::~StateTopology() = default;

// ---------------------------------------------------------------------------
// POD translation helpers. The @ref StateId layout is intentionally
// identical to @c vigine::core::graph::NodeId so the translation is a plain
// field-for-field copy; INV-11 allows it here because the conversion
// lives entirely inside the wrapper implementation.
// ---------------------------------------------------------------------------

vigine::core::graph::NodeId StateTopology::toNodeId(StateId state) noexcept
{
    return vigine::core::graph::NodeId{state.index, state.generation};
}

StateId StateTopology::toStateId(vigine::core::graph::NodeId node) noexcept
{
    return StateId{node.index, node.generation};
}

// ---------------------------------------------------------------------------
// State lifecycle.
// ---------------------------------------------------------------------------

StateId StateTopology::addState()
{
    // The base graph never returns an invalid generation from addNode, so
    // the fresh @ref StateId is always valid. Passing the node back as an
    // @c INode unique_ptr follows the IdStamp handshake — the node captures
    // the assigned id during insert.
    auto                        node = std::make_unique<StateNode>();
    const vigine::core::graph::NodeId nid  = addNode(std::move(node));
    return toStateId(nid);
}

bool StateTopology::hasState(StateId state) const noexcept
{
    if (!state.valid())
    {
        return false;
    }
    const vigine::core::graph::INode *n = node(toNodeId(state));
    return n != nullptr && n->kind() == vigine::fsm::kind::State;
}

// ---------------------------------------------------------------------------
// Hierarchy.
// ---------------------------------------------------------------------------

Result StateTopology::addChildEdge(StateId parent, StateId child)
{
    if (!parent.valid() || !child.valid())
    {
        return Result(Result::Code::Error, "invalid state id");
    }
    if (parent == child)
    {
        return Result(Result::Code::Error, "state cannot be its own parent");
    }

    // Serialise the probe-existing-parent + add-edge sequence under
    // the wrapper-side hierarchy mutex. The graph layer takes its
    // own lock per individual call, but without this outer mutex two
    // racing callers could both pass the `existingParents.empty()`
    // gate and both insert a `ChildOf` edge on the same child —
    // which violates the single-parent invariant every downstream
    // dispatch path relies on.
    std::lock_guard<std::mutex> lock{_hierarchyMutex};

    const vigine::core::graph::NodeId parentNode = toNodeId(parent);
    const vigine::core::graph::NodeId childNode  = toNodeId(child);

    if (!query().hasNode(parentNode))
    {
        return Result(Result::Code::Error, "parent state not registered");
    }
    if (!query().hasNode(childNode))
    {
        return Result(Result::Code::Error, "child state not registered");
    }

    // Reject a registration that would introduce a cycle. Because the
    // edge direction is child->parent, a cycle appears if @p child is
    // already an ancestor of @p parent. The ancestor walk below is O(depth)
    // and bounded by @ref kMaxHierarchyDepth, so the check is cheap.
    if (isAncestorOf(child, parent))
    {
        return Result(Result::Code::Error, "child-of edge would introduce a cycle");
    }

    // Reject multi-parent registrations: a state with two outgoing
    // @c ChildOf edges is ambiguous for bubble traversal. The wrapper
    // keeps the single-parent invariant explicit at registration time.
    const auto existingParents
        = query().outEdgesOfKind(childNode, vigine::fsm::edge_kind::ChildOf);
    if (!existingParents.empty())
    {
        return Result(Result::Code::Error, "child already has a parent");
    }

    auto                        edgePtr = std::make_unique<ChildOfEdge>(childNode, parentNode);
    const vigine::core::graph::EdgeId eid     = addEdge(std::move(edgePtr));
    if (!eid.valid())
    {
        return Result(Result::Code::Error, "failed to add child-of edge");
    }
    return Result();
}

StateId StateTopology::parentOf(StateId state) const
{
    if (!state.valid())
    {
        return StateId{};
    }
    const vigine::core::graph::NodeId nid = toNodeId(state);
    if (!query().hasNode(nid))
    {
        return StateId{};
    }

    const auto parents = query().outEdgesOfKind(nid, vigine::fsm::edge_kind::ChildOf);
    if (parents.empty())
    {
        return StateId{};
    }
    const vigine::core::graph::IEdge *e = edge(parents.front());
    if (e == nullptr)
    {
        return StateId{};
    }
    return toStateId(e->to());
}

bool StateTopology::isAncestorOf(StateId ancestor, StateId descendant) const
{
    if (!ancestor.valid() || !descendant.valid())
    {
        return false;
    }

    // Walk the parent chain from @p descendant upward. Stop at the root
    // (no further parent), when we find @p ancestor, or when the walk
    // exceeds the defensive depth cap.
    StateId cursor = parentOf(descendant);
    for (std::size_t depth = 0; depth < kMaxHierarchyDepth; ++depth)
    {
        if (!cursor.valid())
        {
            return false;
        }
        if (cursor == ancestor)
        {
            return true;
        }
        cursor = parentOf(cursor);
    }
    return false;
}

} // namespace vigine::statemachine
