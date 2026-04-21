#include "vigine/graph/abstractgraph.h"

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
// Hash adapter so NodeId can be stored in unordered containers.
struct NodeIdHasher
{
    std::size_t operator()(NodeId id) const noexcept
    {
        return (static_cast<std::size_t>(id.index) << 32) ^ static_cast<std::size_t>(id.generation);
    }
};

using NodeIdSet = std::unordered_set<NodeId, NodeIdHasher>;

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
    const INode *ptr = resolve(graph, id);
    if (!ptr)
    {
        // Node removed between snapshot and visit — safe to skip.
        return NodeStep::Prune;
    }
    const VisitResult result = visitor.onNode(*ptr);
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
    const IEdge *ptr = resolve(graph, id);
    if (!ptr)
    {
        *prune = true;
        return true;
    }
    const VisitResult result = visitor.onEdge(*ptr);
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
                    return Result(Result::Code::Error, "traversal stopped");
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
                        return Result(Result::Code::Error, "traversal stopped");
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
                    return Result(Result::Code::Error, "traversal stopped");
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
                        return Result(Result::Code::Error, "traversal stopped");
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
            std::unordered_map<NodeId, std::size_t, NodeIdHasher> indegree;
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
                    return Result(Result::Code::Error, "traversal stopped");
                }
                if (step == NodeStep::Prune)
                {
                    continue;
                }
                const std::vector<EdgeId> outs = snapshotOutEdges(nid);
                for (EdgeId eid : outs)
                {
                    bool       prune = false;
                    const bool cont  = visitEdge(*this, eid, visitor, &prune);
                    if (!cont)
                    {
                        return Result(Result::Code::Error, "traversal stopped");
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
                    return Result(Result::Code::Error, "traversal stopped");
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
