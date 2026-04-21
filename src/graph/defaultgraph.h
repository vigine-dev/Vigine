#pragma once

#include <memory>
#include <utility>

#include "vigine/graph/abstractgraph.h"
#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/iedgedata.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/kind.h"
#include "vigine/graph/nodeid.h"

namespace vigine::graph
{
/**
 * @brief Default concrete implementation of @ref IGraph.
 *
 * Closes the inheritance chain on @ref AbstractGraph and provides no
 * storage or behaviour of its own; all mechanics live on @ref AbstractGraph.
 * The @c final keyword prevents further subclassing so that
 * @ref createGraph has one well-defined concrete return type.
 */
class DefaultGraph final : public AbstractGraph
{
  public:
    DefaultGraph()           = default;
    ~DefaultGraph() override = default;
};

namespace internal
{
/**
 * @brief Minimal concrete @ref INode used by the smoke test and by any
 *        ad-hoc client that only cares about topology.
 *
 * Inherits @ref AbstractGraph::IdStamp so that the graph can feed the
 * generational id back to the node at insertion time. Wrappers supply
 * their own @ref INode subclasses carrying wrapper-specific state; they
 * are not required to rely on @ref NodeImpl.
 */
class NodeImpl final
    : public INode
    , public AbstractGraph::IdStamp
{
  public:
    explicit NodeImpl(NodeKind kind) noexcept : _kind{kind} {}

    [[nodiscard]] NodeId   id() const noexcept override { return _id; }
    [[nodiscard]] NodeKind kind() const noexcept override { return _kind; }

    void onGraphIdAssigned(NodeId nodeId, EdgeId edgeId) noexcept override
    {
        _id = nodeId;
        (void)edgeId;
    }

  private:
    NodeId   _id{};
    NodeKind _kind{};
};

/**
 * @brief Minimal concrete @ref IEdge paired with @ref NodeImpl.
 */
class EdgeImpl final
    : public IEdge
    , public AbstractGraph::IdStamp
{
  public:
    EdgeImpl(NodeId from, NodeId to, EdgeKind kind, std::unique_ptr<IEdgeData> data) noexcept
        : _from{from}, _to{to}, _kind{kind}, _data{std::move(data)}
    {
    }

    [[nodiscard]] EdgeId           id() const noexcept override { return _id; }
    [[nodiscard]] EdgeKind         kind() const noexcept override { return _kind; }
    [[nodiscard]] NodeId           from() const noexcept override { return _from; }
    [[nodiscard]] NodeId           to() const noexcept override { return _to; }
    [[nodiscard]] const IEdgeData *data() const noexcept override { return _data.get(); }

    void onGraphIdAssigned(NodeId nodeId, EdgeId edgeId) noexcept override
    {
        _id = edgeId;
        (void)nodeId;
    }

  private:
    EdgeId                     _id{};
    NodeId                     _from{};
    NodeId                     _to{};
    EdgeKind                   _kind{};
    std::unique_ptr<IEdgeData> _data{};
};

[[nodiscard]] inline std::unique_ptr<INode> makeNode(NodeKind kind)
{
    return std::make_unique<NodeImpl>(kind);
}

[[nodiscard]] inline std::unique_ptr<IEdge>
    makeEdge(NodeId from, NodeId to, EdgeKind kind, std::unique_ptr<IEdgeData> data = nullptr)
{
    return std::make_unique<EdgeImpl>(from, to, kind, std::move(data));
}

} // namespace internal

} // namespace vigine::graph
