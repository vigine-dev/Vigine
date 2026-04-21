#include "vigine/graph/abstractgraph.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace vigine::graph
{
// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

AbstractGraph::AbstractGraph() : _query(*this) {}

AbstractGraph::~AbstractGraph() = default;

// ---------------------------------------------------------------------------
// Id stamping — concrete nodes / edges that want their own id inherit
// IdStamp and receive a single callback from the graph when their slot is
// ready. Callers that do not inherit IdStamp get no callback and report
// NodeId{} / EdgeId{} from id(); the graph still tracks them internally.
// ---------------------------------------------------------------------------

void AbstractGraph::stampOnInsert(INode &node, NodeId id) noexcept
{
    if (auto *stamp = dynamic_cast<IdStamp *>(&node))
    {
        stamp->onGraphIdAssigned(id, EdgeId{});
    }
}

void AbstractGraph::stampOnInsert(IEdge &edge, EdgeId id) noexcept
{
    if (auto *stamp = dynamic_cast<IdStamp *>(&edge))
    {
        stamp->onGraphIdAssigned(NodeId{}, id);
    }
}

// ---------------------------------------------------------------------------
// Node lifecycle.
// ---------------------------------------------------------------------------

NodeId AbstractGraph::addNode(std::unique_ptr<INode> node)
{
    if (!node)
    {
        return NodeId{};
    }
    std::unique_lock lock(_mutex);

    const std::uint32_t index      = _nextNodeIndex++;
    const std::uint32_t generation = 1;
    NodeSlot            slot;
    slot.node       = std::move(node);
    slot.generation = generation;
    const NodeId id{index, generation};
    stampOnInsert(*slot.node, id);
    _nodes.emplace(index, std::move(slot));
    _version.fetch_add(1, std::memory_order_release);
    return id;
}

Result AbstractGraph::removeNode(NodeId id)
{
    if (!id.valid())
    {
        return Result(Result::Code::Error, "invalid node id");
    }
    std::unique_lock lock(_mutex);
    const auto       it = _nodes.find(id.index);
    if (it == _nodes.end() || it->second.generation != id.generation || !it->second.node)
    {
        return Result(Result::Code::Error, "stale node id");
    }

    const std::vector<EdgeId> outCopy = it->second.outEdges;
    const std::vector<EdgeId> inCopy  = it->second.inEdges;
    for (EdgeId eid : outCopy)
    {
        eraseEdgeLocked(eid);
    }
    for (EdgeId eid : inCopy)
    {
        eraseEdgeLocked(eid);
    }

    auto &slot = it->second;
    slot.node.reset();
    slot.outEdges.clear();
    slot.inEdges.clear();
    ++slot.generation;
    _version.fetch_add(1, std::memory_order_release);
    return Result();
}

const INode *AbstractGraph::node(NodeId id) const noexcept
{
    if (!id.valid())
    {
        return nullptr;
    }
    std::shared_lock lock(_mutex);
    const auto       it = _nodes.find(id.index);
    if (it == _nodes.end() || it->second.generation != id.generation || !it->second.node)
    {
        return nullptr;
    }
    return it->second.node.get();
}

// ---------------------------------------------------------------------------
// Edge lifecycle.
// ---------------------------------------------------------------------------

EdgeId AbstractGraph::addEdge(std::unique_ptr<IEdge> edge)
{
    if (!edge)
    {
        return EdgeId{};
    }
    const NodeId fromId = edge->from();
    const NodeId toId   = edge->to();

    std::unique_lock lock(_mutex);
    const auto       fromIt = _nodes.find(fromId.index);
    const auto       toIt   = _nodes.find(toId.index);
    if (fromIt == _nodes.end() || fromIt->second.generation != fromId.generation || !fromIt->second.node
        || toIt == _nodes.end() || toIt->second.generation != toId.generation || !toIt->second.node)
    {
        return EdgeId{};
    }

    const std::uint32_t index      = _nextEdgeIndex++;
    const std::uint32_t generation = 1;
    EdgeSlot            slot;
    slot.edge       = std::move(edge);
    slot.generation = generation;
    const EdgeId id{index, generation};
    stampOnInsert(*slot.edge, id);

    fromIt->second.outEdges.push_back(id);
    toIt->second.inEdges.push_back(id);
    _edges.emplace(index, std::move(slot));
    _version.fetch_add(1, std::memory_order_release);
    return id;
}

Result AbstractGraph::removeEdge(EdgeId id)
{
    if (!id.valid())
    {
        return Result(Result::Code::Error, "invalid edge id");
    }
    std::unique_lock lock(_mutex);
    if (!eraseEdgeLocked(id))
    {
        return Result(Result::Code::Error, "stale edge id");
    }
    _version.fetch_add(1, std::memory_order_release);
    return Result();
}

const IEdge *AbstractGraph::edge(EdgeId id) const noexcept
{
    if (!id.valid())
    {
        return nullptr;
    }
    std::shared_lock lock(_mutex);
    const auto       it = _edges.find(id.index);
    if (it == _edges.end() || it->second.generation != id.generation || !it->second.edge)
    {
        return nullptr;
    }
    return it->second.edge.get();
}

// ---------------------------------------------------------------------------
// Query / observability.
// ---------------------------------------------------------------------------

const IGraphQuery &AbstractGraph::query() const noexcept
{
    return _query;
}

std::size_t AbstractGraph::nodeCount() const noexcept
{
    std::shared_lock lock(_mutex);
    std::size_t      live = 0;
    for (const auto &[_, slot] : _nodes)
    {
        if (slot.node)
        {
            ++live;
        }
    }
    return live;
}

std::size_t AbstractGraph::edgeCount() const noexcept
{
    std::shared_lock lock(_mutex);
    std::size_t      live = 0;
    for (const auto &[_, slot] : _edges)
    {
        if (slot.edge)
        {
            ++live;
        }
    }
    return live;
}

// ---------------------------------------------------------------------------
// Snapshot helpers used by traversal / export.
// ---------------------------------------------------------------------------

std::vector<NodeId> AbstractGraph::snapshotLiveNodes() const
{
    std::shared_lock    lock(_mutex);
    std::vector<NodeId> ids;
    ids.reserve(_nodes.size());
    for (const auto &[index, slot] : _nodes)
    {
        if (slot.node)
        {
            ids.push_back(NodeId{index, slot.generation});
        }
    }
    return ids;
}

std::vector<EdgeId> AbstractGraph::snapshotOutEdges(NodeId id) const
{
    std::shared_lock lock(_mutex);
    const auto       it = _nodes.find(id.index);
    if (it == _nodes.end() || it->second.generation != id.generation)
    {
        return {};
    }
    return it->second.outEdges;
}

// ---------------------------------------------------------------------------
// buildSnapshot — captures the full adjacency shape of the graph under a
// single shared lock. The traversal driver and the structural query
// methods use the returned value to run their algorithms without holding
// the mutex during the actual work.
// ---------------------------------------------------------------------------

AbstractGraph::Snapshot AbstractGraph::buildSnapshot() const
{
    Snapshot         s;
    std::shared_lock lock(_mutex);
    s.nodes.reserve(_nodes.size());
    for (const auto &[index, slot] : _nodes)
    {
        if (!slot.node)
        {
            continue;
        }
        NodeId nid{index, slot.generation};
        s.nodes.push_back(nid);
        s.outByKey[Snapshot::nodeKey(nid)] = slot.outEdges;
        s.inByKey[Snapshot::nodeKey(nid)]  = slot.inEdges;
    }
    for (const auto &[index, slot] : _edges)
    {
        if (!slot.edge)
        {
            continue;
        }
        EdgeId eid{index, slot.generation};
        s.edgeEndpoints[Snapshot::edgeKey(eid)] = {slot.edge->from(), slot.edge->to()};
    }
    return s;
}

// ---------------------------------------------------------------------------
// eraseEdgeLocked — removes a single edge while the caller already holds
// the exclusive lock. Updates both endpoints' adjacency lists so that the
// cascade in removeNode does not leave dangling references. Reports
// whether the edge existed.
// ---------------------------------------------------------------------------

bool AbstractGraph::eraseEdgeLocked(EdgeId id)
{
    const auto it = _edges.find(id.index);
    if (it == _edges.end() || it->second.generation != id.generation || !it->second.edge)
    {
        return false;
    }
    const NodeId fromId = it->second.edge->from();
    const NodeId toId   = it->second.edge->to();

    const auto detach = [&](NodeId nid, bool outgoing)
    {
        const auto nit = _nodes.find(nid.index);
        if (nit == _nodes.end() || nit->second.generation != nid.generation)
        {
            return;
        }
        auto &list = outgoing ? nit->second.outEdges : nit->second.inEdges;
        list.erase(std::remove(list.begin(), list.end(), id), list.end());
    };
    detach(fromId, /*outgoing=*/true);
    detach(toId, /*outgoing=*/false);

    it->second.edge.reset();
    ++it->second.generation;
    return true;
}

// ---------------------------------------------------------------------------
// QueryImpl — the read-only surface delegates back into the owning graph
// under shared locks. Each method builds its own container.
// ---------------------------------------------------------------------------

AbstractGraph::QueryImpl::QueryImpl(const AbstractGraph &graph) : _graph(graph) {}

bool AbstractGraph::QueryImpl::hasNode(NodeId id) const noexcept
{
    return _graph.node(id) != nullptr;
}

bool AbstractGraph::QueryImpl::hasEdge(EdgeId id) const noexcept
{
    return _graph.edge(id) != nullptr;
}

std::vector<EdgeId> AbstractGraph::QueryImpl::outEdges(NodeId id) const
{
    std::shared_lock lock(_graph._mutex);
    const auto       it = _graph._nodes.find(id.index);
    if (it == _graph._nodes.end() || it->second.generation != id.generation)
    {
        return {};
    }
    return it->second.outEdges;
}

std::vector<EdgeId> AbstractGraph::QueryImpl::inEdges(NodeId id) const
{
    std::shared_lock lock(_graph._mutex);
    const auto       it = _graph._nodes.find(id.index);
    if (it == _graph._nodes.end() || it->second.generation != id.generation)
    {
        return {};
    }
    return it->second.inEdges;
}

std::vector<EdgeId> AbstractGraph::QueryImpl::outEdgesOfKind(NodeId id, EdgeKind kind) const
{
    std::shared_lock lock(_graph._mutex);
    const auto       it = _graph._nodes.find(id.index);
    if (it == _graph._nodes.end() || it->second.generation != id.generation)
    {
        return {};
    }
    std::vector<EdgeId> out;
    out.reserve(it->second.outEdges.size());
    for (EdgeId eid : it->second.outEdges)
    {
        const auto eit = _graph._edges.find(eid.index);
        if (eit != _graph._edges.end() && eit->second.generation == eid.generation && eit->second.edge && eit->second.edge->kind() == kind)
        {
            out.push_back(eid);
        }
    }
    return out;
}

std::vector<EdgeId> AbstractGraph::QueryImpl::inEdgesOfKind(NodeId id, EdgeKind kind) const
{
    std::shared_lock lock(_graph._mutex);
    const auto       it = _graph._nodes.find(id.index);
    if (it == _graph._nodes.end() || it->second.generation != id.generation)
    {
        return {};
    }
    std::vector<EdgeId> out;
    out.reserve(it->second.inEdges.size());
    for (EdgeId eid : it->second.inEdges)
    {
        const auto eit = _graph._edges.find(eid.index);
        if (eit != _graph._edges.end() && eit->second.generation == eid.generation && eit->second.edge && eit->second.edge->kind() == kind)
        {
            out.push_back(eid);
        }
    }
    return out;
}

} // namespace vigine::graph
