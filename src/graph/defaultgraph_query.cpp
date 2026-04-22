#include "vigine/graph/abstractgraph.h"

#include "graph/nodeid_hasher.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vigine::graph
{
namespace
{
using NodeIdSet = std::unordered_set<NodeId, internal::NodeIdHasher>;

template <class Value>
using NodeIdMap = std::unordered_map<NodeId, Value, internal::NodeIdHasher>;

} // namespace

// ---------------------------------------------------------------------------
// shortestPath — unweighted BFS on the snapshot. Returns nullopt when
// either endpoint is absent or when no path exists.
// ---------------------------------------------------------------------------

std::optional<std::vector<NodeId>>
AbstractGraph::QueryImpl::shortestPath(NodeId from, NodeId to) const
{
    if (!hasNode(from) || !hasNode(to))
    {
        return std::nullopt;
    }
    if (from == to)
    {
        return std::vector<NodeId>{from};
    }

    const Snapshot snap = _graph.buildSnapshot();

    NodeIdMap<NodeId> parent;
    parent[from] = NodeId{};
    std::deque<NodeId> queue;
    queue.push_back(from);
    bool found = false;
    while (!queue.empty())
    {
        const NodeId current = queue.front();
        queue.pop_front();
        if (current == to)
        {
            found = true;
            break;
        }
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
            const NodeId neighbour = endpointsIt->second.to;
            if (parent.emplace(neighbour, current).second)
            {
                queue.push_back(neighbour);
            }
        }
    }

    if (!found)
    {
        return std::nullopt;
    }

    std::vector<NodeId> path;
    NodeId              cursor = to;
    while (cursor.valid())
    {
        path.push_back(cursor);
        if (cursor == from)
        {
            break;
        }
        const auto it = parent.find(cursor);
        if (it == parent.end())
        {
            break;
        }
        cursor = it->second;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ---------------------------------------------------------------------------
// connectedComponents — treats the graph as undirected. Walks in and out
// edges indiscriminately so that a purely incoming neighbourhood is still
// linked to its reachable component.
// ---------------------------------------------------------------------------

std::vector<std::vector<NodeId>>
AbstractGraph::QueryImpl::connectedComponents() const
{
    const Snapshot snap = _graph.buildSnapshot();

    NodeIdSet                        seen;
    std::vector<std::vector<NodeId>> components;
    for (const NodeId &seed : snap.nodes)
    {
        if (seen.count(seed))
        {
            continue;
        }
        std::vector<NodeId> component;
        std::deque<NodeId>  frontier;
        frontier.push_back(seed);
        seen.insert(seed);
        while (!frontier.empty())
        {
            const NodeId current = frontier.front();
            frontier.pop_front();
            component.push_back(current);

            const auto walk = [&](const std::vector<EdgeId> &edgesForNode, bool outgoing)
            {
                for (EdgeId eid : edgesForNode)
                {
                    const auto endpointsIt = snap.edgeEndpoints.find(Snapshot::edgeKey(eid));
                    if (endpointsIt == snap.edgeEndpoints.end())
                    {
                        continue;
                    }
                    const NodeId other =
                        outgoing ? endpointsIt->second.to : endpointsIt->second.from;
                    if (seen.insert(other).second)
                    {
                        frontier.push_back(other);
                    }
                }
            };
            const auto outIt = snap.outByKey.find(Snapshot::nodeKey(current));
            if (outIt != snap.outByKey.end())
            {
                walk(outIt->second, /*outgoing=*/true);
            }
            const auto inIt = snap.inByKey.find(Snapshot::nodeKey(current));
            if (inIt != snap.inByKey.end())
            {
                walk(inIt->second, /*outgoing=*/false);
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

// ---------------------------------------------------------------------------
// hasCycle — iterative DFS with white/gray/black colouring.
// ---------------------------------------------------------------------------

bool AbstractGraph::QueryImpl::hasCycle() const
{
    const Snapshot snap = _graph.buildSnapshot();
    enum class Colour
    {
        White,
        Gray,
        Black
    };
    NodeIdMap<Colour> colour;
    for (const NodeId &nid : snap.nodes)
    {
        colour[nid] = Colour::White;
    }
    for (const NodeId &seed : snap.nodes)
    {
        if (colour[seed] != Colour::White)
        {
            continue;
        }
        struct Frame
        {
            NodeId      node;
            std::size_t next{0};
        };
        std::vector<Frame> stack;
        stack.push_back({seed, 0});
        colour[seed] = Colour::Gray;
        while (!stack.empty())
        {
            Frame     &top   = stack.back();
            const auto outIt = snap.outByKey.find(Snapshot::nodeKey(top.node));
            if (outIt == snap.outByKey.end() || top.next >= outIt->second.size())
            {
                colour[top.node] = Colour::Black;
                stack.pop_back();
                continue;
            }
            const EdgeId eid = outIt->second[top.next++];
            const auto   endpointsIt = snap.edgeEndpoints.find(Snapshot::edgeKey(eid));
            if (endpointsIt == snap.edgeEndpoints.end())
            {
                continue;
            }
            const NodeId target = endpointsIt->second.to;
            const auto   cit    = colour.find(target);
            if (cit == colour.end())
            {
                continue;
            }
            if (cit->second == Colour::Gray)
            {
                return true;
            }
            if (cit->second == Colour::White)
            {
                cit->second = Colour::Gray;
                stack.push_back({target, 0});
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// topologicalOrder — Kahn's algorithm. Returns nullopt on cycle.
// ---------------------------------------------------------------------------

std::optional<std::vector<NodeId>>
AbstractGraph::QueryImpl::topologicalOrder() const
{
    const Snapshot         snap = _graph.buildSnapshot();
    NodeIdMap<std::size_t> indegree;
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
        return std::nullopt;
    }
    return order;
}

} // namespace vigine::graph
