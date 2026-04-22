#include "taskflow/taskorchestrator.h"

#include <cstdint>
#include <memory>

#include "vigine/graph/abstractgraph.h"
#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/iedgedata.h"
#include "vigine/graph/igraphquery.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"
#include "vigine/result.h"
#include "vigine/taskflow/kind.h"
#include "vigine/taskflow/resultcode.h"
#include "vigine/taskflow/routemode.h"
#include "vigine/taskflow/taskid.h"

namespace vigine::taskflow
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
 * @brief Stable runtime identifier for the @ref TransitionData payload
 *        attached to every transition edge.
 *
 * The constant lives in the anonymous namespace so no other translation
 * unit can accidentally collide with it. The value is arbitrary within
 * the space of @c std::uint32_t; the only contract is that every
 * transition edge reports exactly this value from its
 * @ref vigine::graph::IEdgeData::dataTypeId override.
 */
inline constexpr std::uint32_t kTransitionDataTypeId = 0x74665452U; // "tfTR"

/**
 * @brief Task vertex stored inside the specialised task orchestrator.
 *
 * Carries only the @c vigine::taskflow::kind::Task tag and cooperates
 * with @c AbstractGraph::IdStamp so the assigned generational id
 * flows back to @c id() without a round-trip through the graph.
 */
class TaskNode final
    : public vigine::graph::INode
    , public vigine::graph::AbstractGraph::IdStamp
{
  public:
    TaskNode() = default;

    [[nodiscard]] vigine::graph::NodeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::graph::NodeKind kind() const noexcept override
    {
        return vigine::taskflow::kind::Task;
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
 * @brief Payload attached to a transition edge.
 *
 * Carries the @ref ResultCode that triggers the transition and the
 * @ref RouteMode that determines how the wrapper dispatches when
 * several edges share the same @c (source, code) pair. The payload is
 * immutable after construction; the orchestrator inspects it during
 * registration to enforce the "one @ref RouteMode per pair"
 * invariant.
 */
class TransitionData final : public vigine::graph::IEdgeData
{
  public:
    TransitionData(ResultCode code, RouteMode mode) noexcept
        : _code{code}, _mode{mode}
    {
    }

    [[nodiscard]] std::uint32_t dataTypeId() const noexcept override
    {
        return kTransitionDataTypeId;
    }

    [[nodiscard]] ResultCode code() const noexcept { return _code; }
    [[nodiscard]] RouteMode  mode() const noexcept { return _mode; }

  private:
    ResultCode _code;
    RouteMode  _mode;
};

/**
 * @brief Directed transition edge from a source task to a registered
 *        next task.
 *
 * Owns its @ref TransitionData payload and exposes it through the
 * base class's @c data() hook so the wrapper implementation can read
 * the @ref ResultCode and @ref RouteMode back during queries.
 */
class TransitionEdge final
    : public vigine::graph::IEdge
    , public vigine::graph::AbstractGraph::IdStamp
{
  public:
    TransitionEdge(
        vigine::graph::NodeId from,
        vigine::graph::NodeId to,
        ResultCode            code,
        RouteMode             mode) noexcept
        : _from{from}
        , _to{to}
        , _data{code, mode}
    {
    }

    [[nodiscard]] vigine::graph::EdgeId id() const noexcept override { return _id; }
    [[nodiscard]] vigine::graph::EdgeKind kind() const noexcept override
    {
        return vigine::taskflow::edge_kind::Transition;
    }
    [[nodiscard]] vigine::graph::NodeId from() const noexcept override { return _from; }
    [[nodiscard]] vigine::graph::NodeId to() const noexcept override { return _to; }
    [[nodiscard]] const vigine::graph::IEdgeData *data() const noexcept override
    {
        return &_data;
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
    TransitionData        _data;
};

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

TaskOrchestrator::TaskOrchestrator() = default;

TaskOrchestrator::~TaskOrchestrator() = default;

// ---------------------------------------------------------------------------
// POD translation helpers. The @ref TaskId layout is intentionally
// identical to @c vigine::graph::NodeId so the translation is a plain
// field-for-field copy; INV-11 allows it here because the conversion
// lives entirely inside the wrapper implementation.
// ---------------------------------------------------------------------------

vigine::graph::NodeId TaskOrchestrator::toNodeId(TaskId task) noexcept
{
    return vigine::graph::NodeId{task.index, task.generation};
}

TaskId TaskOrchestrator::toTaskId(vigine::graph::NodeId node) noexcept
{
    return TaskId{node.index, node.generation};
}

// ---------------------------------------------------------------------------
// Task lifecycle.
// ---------------------------------------------------------------------------

TaskId TaskOrchestrator::addTask()
{
    // The base graph never returns an invalid generation from addNode, so
    // the fresh @ref TaskId is always valid. Passing the node back as an
    // @c INode unique_ptr follows the IdStamp handshake — the node captures
    // the assigned id during insert.
    auto                        node = std::make_unique<TaskNode>();
    const vigine::graph::NodeId nid  = addNode(std::move(node));
    return toTaskId(nid);
}

bool TaskOrchestrator::hasTask(TaskId task) const noexcept
{
    if (!task.valid())
    {
        return false;
    }
    const vigine::graph::INode *n = node(toNodeId(task));
    return n != nullptr && n->kind() == vigine::taskflow::kind::Task;
}

// ---------------------------------------------------------------------------
// Transitions.
// ---------------------------------------------------------------------------

RouteMode TaskOrchestrator::storedModeFor(
    TaskId     source,
    ResultCode code,
    RouteMode  fallback) const
{
    const vigine::graph::NodeId srcNode = toNodeId(source);
    if (!query().hasNode(srcNode))
    {
        return fallback;
    }

    // Walk the outgoing transition edges of @p source and pick the first
    // edge that carries the matching @ref ResultCode. That edge's stored
    // @ref RouteMode is the canonical mode for the pair; later
    // registrations for the same pair must repeat it.
    const auto edges
        = query().outEdgesOfKind(srcNode, vigine::taskflow::edge_kind::Transition);
    for (const auto eid : edges)
    {
        const vigine::graph::IEdge *e = edge(eid);
        if (e == nullptr)
        {
            continue;
        }
        const vigine::graph::IEdgeData *d = e->data();
        if (d == nullptr || d->dataTypeId() != kTransitionDataTypeId)
        {
            continue;
        }
        const auto *td = static_cast<const TransitionData *>(d);
        if (td->code() == code)
        {
            return td->mode();
        }
    }
    return fallback;
}

Result TaskOrchestrator::addTransition(
    TaskId    source,
    ResultCode code,
    TaskId    next,
    RouteMode mode)
{
    if (!source.valid() || !next.valid())
    {
        return Result(Result::Code::Error, "invalid task id");
    }
    if (source == next)
    {
        return Result(Result::Code::Error, "task cannot transition to itself");
    }

    const vigine::graph::NodeId srcNode  = toNodeId(source);
    const vigine::graph::NodeId nextNode = toNodeId(next);

    if (!query().hasNode(srcNode))
    {
        return Result(Result::Code::Error, "source task not registered");
    }
    if (!query().hasNode(nextNode))
    {
        return Result(Result::Code::Error, "next task not registered");
    }

    // Enforce the "one @ref RouteMode per (source, code) pair" invariant.
    // The first registration locks the mode; every subsequent registration
    // for the same pair must repeat the same mode or the wrapper rejects
    // it so routing stays unambiguous.
    const RouteMode stored = storedModeFor(source, code, mode);
    if (stored != mode)
    {
        return Result(
            Result::Code::Error,
            "conflicting RouteMode for (source, code) pair");
    }

    auto edgePtr
        = std::make_unique<TransitionEdge>(srcNode, nextNode, code, mode);
    const vigine::graph::EdgeId eid = addEdge(std::move(edgePtr));
    if (!eid.valid())
    {
        return Result(Result::Code::Error, "failed to add transition edge");
    }
    return Result();
}

} // namespace vigine::taskflow
