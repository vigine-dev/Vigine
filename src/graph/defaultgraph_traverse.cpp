#include "vigine/graph/abstractgraph.h"

#include "graph/nodeid_hasher.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vigine::graph
{
namespace
{
using NodeIdSet = std::unordered_set<NodeId, internal::NodeIdHasher>;

// ---------------------------------------------------------------------------
// Visitor-facing by-value snapshots.
//
// The driver used to hand the visitor a raw `INode &` / `IEdge &`
// obtained via `graph.node(id)` / `graph.edge(id)` — a non-owning
// pointer that refers directly to the graph's live storage. Visitor
// callbacks are explicitly allowed to mutate the graph (remove the
// current node, rewire edges, ...), so a `removeNode(current)`
// inside `onNode()` would free the storage while the reference was
// still on the stack — undefined behaviour.
//
// We now copy the interface-visible fields into a stack-allocated
// snapshot object before the callback runs. Snapshots satisfy the
// pure-virtual interfaces (`INode`, `IEdge`) without touching the
// live graph. The visitor holds a reference to the snapshot, not
// to the live node/edge; a concurrent mutation cannot invalidate
// the fields the callback reads.
//
// Caveat on `IEdge::data()`: the snapshot carries a `const IEdgeData
// *` that was fetched from the live edge before the callback. The
// `IEdgeData` object itself lives inside the graph-owned `IEdge`;
// if the visitor removes the current edge, the `IEdgeData` pointed
// at goes with it, so the pointer `snapshot.data()` returns
// dangles post-remove. Callers that need the payload must read it
// BEFORE mutating. This is a narrower failure mode than the old
// "every field dangles" shape and matches how pointer-returning
// accessors typically behave in C++.
// ---------------------------------------------------------------------------

class NodeSnapshot final : public INode
{
  public:
    NodeSnapshot(NodeId id, NodeKind kind) noexcept
        : _id(id), _kind(kind)
    {
    }
    [[nodiscard]] NodeId   id()   const noexcept override { return _id; }
    [[nodiscard]] NodeKind kind() const noexcept override { return _kind; }

  private:
    NodeId   _id;
    NodeKind _kind;
};

class EdgeSnapshot final : public IEdge
{
  public:
    EdgeSnapshot(EdgeId           id,
                 EdgeKind         kind,
                 NodeId           from,
                 NodeId           to,
                 const IEdgeData *data) noexcept
        : _id(id), _kind(kind), _from(from), _to(to), _data(data)
    {
    }
    [[nodiscard]] EdgeId           id()   const noexcept override { return _id; }
    [[nodiscard]] EdgeKind         kind() const noexcept override { return _kind; }
    [[nodiscard]] NodeId           from() const noexcept override { return _from; }
    [[nodiscard]] NodeId           to()   const noexcept override { return _to; }
    [[nodiscard]] const IEdgeData *data() const noexcept override { return _data; }

  private:
    EdgeId           _id;
    EdgeKind         _kind;
    NodeId           _from;
    NodeId           _to;
    const IEdgeData *_data;
};

// Re-looks-up a node / edge through the public read path, which handles
// its own locking and generational check.
const INode *resolve(const AbstractGraph &graph, NodeId id)
{
    return graph.node(id);
}

const IEdge *resolve(const AbstractGraph &graph, EdgeId id)
{
    return graph.edge(id);
}

// Drives the onNode callback and translates the visitor's return into the
// next driver step.
enum class NodeStep
{
    Descend,
    Prune,
    Stop
};

NodeStep visitNode(const AbstractGraph &graph, NodeId id, IGraphVisitor &visitor)
{
    // Copy the interface-visible fields out of the live node BEFORE
    // handing anything to the visitor. A visitor that mutates the
    // graph inside its own `onNode` (e.g. `removeNode(current)`)
    // would invalidate a raw `INode &` we'd otherwise pass through.
    NodeKind kind{};
    {
        const INode *ptr = resolve(graph, id);
        if (!ptr)
        {
            // Node removed between snapshot and visit — safe to skip.
            return NodeStep::Prune;
        }
        kind = ptr->kind();
    }
    NodeSnapshot      snapshot{id, kind};
    const VisitResult result = visitor.onNode(snapshot);
    switch (result)
    {
        case VisitResult::Continue: return NodeStep::Descend;
        case VisitResult::Skip:     return NodeStep::Prune;
        case VisitResult::Stop:     return NodeStep::Stop;
    }
    return NodeStep::Descend;
}

// Drives the onEdge callback. Returns true to keep walking, false to stop;
// *prune is set to true if the edge should be ignored but walking should
// continue on sibling edges.
bool visitEdge(const AbstractGraph &graph, EdgeId id, IGraphVisitor &visitor, bool *prune)
{
    *prune = false;
    // Snapshot the edge's interface fields so a visitor that removes
    // the current edge from inside onEdge() cannot see a dangling
    // IEdge reference. See the snapshot-type comment at the top of
    // this TU for the data()-pointer caveat.
    EdgeKind         kind{};
    NodeId           from{};
    NodeId           to{};
    const IEdgeData *data = nullptr;
    {
        const IEdge *ptr = resolve(graph, id);
        if (!ptr)
        {
            *prune = true;
            return true;
        }
        kind = ptr->kind();
        from = ptr->from();
        to   = ptr->to();
        data = ptr->data();
    }
    EdgeSnapshot      snapshot{id, kind, from, to, data};
    const VisitResult result = visitor.onEdge(snapshot);
    switch (result)
    {
        case VisitResult::Continue: return true;
        case VisitResult::Skip:     *prune = true; return true;
        case VisitResult::Stop:     return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// traverse — dispatches on the requested mode. The driver for each mode
// runs against a snapshot of the relevant structural data so that the
// visitor callbacks run lock-free.
// ---------------------------------------------------------------------------

Result AbstractGraph::traverse(NodeId startNode, TraverseMode mode, IGraphVisitor &visitor)
{
    switch (mode)
    {
        case TraverseMode::DepthFirst:
        {
            if (!startNode.valid() || !node(startNode))
            {
                return Result(Result::Code::Error, "invalid start node");
            }
            NodeIdSet           visited;
            std::vector<NodeId> stack;
            stack.push_back(startNode);
            while (!stack.empty())
            {
                const NodeId current = stack.back();
                stack.pop_back();
                if (!visited.insert(current).second)
                {
                    continue;
                }
                const NodeStep step = visitNode(*this, current, visitor);
                if (step == NodeStep::Stop)
                {
                    return Result();
                }
                if (step == NodeStep::Prune)
                {
                    continue;
                }
                const std::vector<EdgeId> outs = snapshotOutEdges(current);
                // Reverse-push so that the first outgoing edge is processed
                // first by the LIFO stack.
                for (auto it = outs.rbegin(); it != outs.rend(); ++it)
                {
                    bool       prune = false;
                    const bool cont  = visitEdge(*this, *it, visitor, &prune);
                    if (!cont)
                    {
                        return Result();
                    }
                    if (prune)
                    {
                        continue;
                    }
                    const IEdge *e = resolve(*this, *it);
                    if (!e)
                    {
                        continue;
                    }
                    if (!visited.count(e->to()))
                    {
                        stack.push_back(e->to());
                    }
                }
            }
            return Result();
        }
        case TraverseMode::BreadthFirst:
        {
            if (!startNode.valid() || !node(startNode))
            {
                return Result(Result::Code::Error, "invalid start node");
            }
            NodeIdSet          visited;
            std::deque<NodeId> queue;
            queue.push_back(startNode);
            visited.insert(startNode);
            while (!queue.empty())
            {
                const NodeId current = queue.front();
                queue.pop_front();
                const NodeStep step = visitNode(*this, current, visitor);
                if (step == NodeStep::Stop)
                {
                    return Result();
                }
                if (step == NodeStep::Prune)
                {
                    continue;
                }
                const std::vector<EdgeId> outs = snapshotOutEdges(current);
                for (EdgeId eid : outs)
                {
                    bool       prune = false;
                    const bool cont  = visitEdge(*this, eid, visitor, &prune);
                    if (!cont)
                    {
                        return Result();
                    }
                    if (prune)
                    {
                        continue;
                    }
                    const IEdge *e = resolve(*this, eid);
                    if (!e)
                    {
                        continue;
                    }
                    if (visited.insert(e->to()).second)
                    {
                        queue.push_back(e->to());
                    }
                }
            }
            return Result();
        }
        case TraverseMode::Topological:
        case TraverseMode::ReverseTopological:
        {
            // Kahn's algorithm. startNode is advisory only — topological
            // traversal walks every reachable component.
            (void)startNode;

            const Snapshot snap = buildSnapshot();
            std::unordered_map<NodeId, std::size_t, internal::NodeIdHasher> indegree;
            for (const NodeId &nid : snap.nodes)
            {
                const auto it = snap.inByKey.find(Snapshot::nodeKey(nid));
                indegree[nid] = it == snap.inByKey.end() ? 0 : it->second.size();
            }

            std::deque<NodeId> ready;
            for (const NodeId &nid : snap.nodes)
            {
                if (indegree[nid] == 0)
                {
                    ready.push_back(nid);
                }
            }

            std::vector<NodeId> order;
            order.reserve(snap.nodes.size());
            while (!ready.empty())
            {
                const NodeId current = ready.front();
                ready.pop_front();
                order.push_back(current);
                const auto outIt = snap.outByKey.find(Snapshot::nodeKey(current));
                if (outIt == snap.outByKey.end())
                {
                    continue;
                }
                for (EdgeId eid : outIt->second)
                {
                    const auto endpointsIt = snap.edgeEndpoints.find(Snapshot::edgeKey(eid));
                    if (endpointsIt == snap.edgeEndpoints.end())
                    {
                        continue;
                    }
                    const NodeId target = endpointsIt->second.to;
                    const auto   it     = indegree.find(target);
                    if (it == indegree.end())
                    {
                        continue;
                    }
                    if (it->second > 0)
                    {
                        --it->second;
                    }
                    if (it->second == 0)
                    {
                        ready.push_back(target);
                    }
                }
            }

            if (order.size() < snap.nodes.size())
            {
                return Result(Result::Code::Error, "cycle detected during topological traversal");
            }

            if (mode == TraverseMode::ReverseTopological)
            {
                std::reverse(order.begin(), order.end());
            }

            for (const NodeId &nid : order)
            {
                const NodeStep step = visitNode(*this, nid, visitor);
                if (step == NodeStep::Stop)
                {
                    return Result();
                }
                if (step == NodeStep::Prune)
                {
                    continue;
                }
                // Read outgoing edges from the snapshot captured above,
                // not from the live graph. Going back to the live graph
                // here would (a) reintroduce the lock contention the
                // snapshot was built to eliminate, and (b) let edges
                // added concurrently after the snapshot slip into the
                // walk — breaking the documented "traversal is isolated
                // from concurrent mutation" contract.
                const auto outIt = snap.outByKey.find(Snapshot::nodeKey(nid));
                if (outIt == snap.outByKey.end())
                {
                    continue;
                }
                for (EdgeId eid : outIt->second)
                {
                    bool       prune = false;
                    const bool cont  = visitEdge(*this, eid, visitor, &prune);
                    if (!cont)
                    {
                        return Result();
                    }
                    if (prune)
                    {
                        continue;
                    }
                }
            }
            return Result();
        }
        case TraverseMode::Custom:
        {
            if (!startNode.valid() || !node(startNode))
            {
                return Result(Result::Code::Error, "invalid start node");
            }
            NodeIdSet visited;
            NodeId    current = startNode;
            while (current.valid())
            {
                if (!visited.insert(current).second)
                {
                    break;
                }
                const NodeStep step = visitNode(*this, current, visitor);
                if (step == NodeStep::Stop)
                {
                    return Result();
                }
                if (step == NodeStep::Prune)
                {
                    break;
                }
                const INode *still = resolve(*this, current);
                if (!still)
                {
                    break;
                }
                current = visitor.nextForCustom(*still);
            }
            return Result();
        }
    }
    return Result(Result::Code::Error, "unknown traverse mode");
}

} // namespace vigine::graph
