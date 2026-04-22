#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "vigine/graph/edgeid.h"
#include "vigine/graph/iedge.h"
#include "vigine/graph/igraphquery.h"
#include "vigine/graph/igraphvisitor.h"
#include "vigine/graph/inode.h"
#include "vigine/graph/nodeid.h"
#include "vigine/graph/traverse_mode.h"
#include "vigine/result.h"

namespace vigine::graph
{
/**
 * @brief Pure-virtual core of the graph substrate.
 *
 * @ref IGraph is the level-0 primitive underneath every wrapper in the
 * engine (messaging, ECS, state machine, task flow). It owns nodes and
 * edges, exposes traversal and query surfaces, and offers a tooling hook
 * for GraphViz export. It knows nothing about higher-level concepts such
 * as entities, components, or messages; those live in wrapper headers.
 *
 * Ownership semantics:
 *   - @ref addNode and @ref addEdge take unique ownership of the
 *     implementation objects.
 *   - @ref node and @ref edge return non-owning raw pointers. Returned
 *     pointers are valid until the next mutation of the graph; callers
 *     that need longer-lived references should copy the identifier and
 *     re-resolve. The base interface does not offer a thread-safety
 *     guarantee: concurrent access (read + mutate, or mutate + mutate)
 *     requires external synchronization unless a concrete implementation
 *     documents stronger guarantees.
 *   - Identifiers are generational (@ref NodeId, @ref EdgeId). Stale
 *     identifiers never alias live slots after removal.
 */
class IGraph
{
  public:
    virtual ~IGraph() = default;

    // ------ Node lifecycle ------

    /**
     * @brief Takes ownership of @p node and returns the generational id.
     *
     * The returned identifier is always valid; implementations never
     * return a generation of zero from @ref addNode. Passing a null
     * pointer is a programming error and is undefined.
     */
    [[nodiscard]] virtual NodeId addNode(std::unique_ptr<INode> node) = 0;

    /**
     * @brief Removes the node addressed by @p id and every attached edge.
     *
     * Idempotent: removing a stale identifier reports a
     * @ref Result::Code::Error status without side effects. On success
     * returns a default-constructed (Success) @ref Result.
     */
    virtual Result removeNode(NodeId id) = 0;

    /**
     * @brief Returns a non-owning view of the node, or `nullptr` when the
     *        identifier is stale.
     */
    [[nodiscard]] virtual const INode *node(NodeId id) const noexcept = 0;

    // ------ Edge lifecycle ------

    /**
     * @brief Takes ownership of @p edge and returns the generational id.
     *
     * Implementations validate that the endpoints addressed by @p edge
     * exist; when they do not, the returned identifier is invalid.
     * Passing a null pointer is a programming error and is undefined —
     * same contract as @ref addNode; callers must hand over a
     * fully-constructed concrete @ref IEdge.
     */
    [[nodiscard]] virtual EdgeId addEdge(std::unique_ptr<IEdge> edge) = 0;

    /**
     * @brief Removes the edge addressed by @p id. Idempotent.
     */
    virtual Result removeEdge(EdgeId id) = 0;

    /**
     * @brief Returns a non-owning view of the edge, or `nullptr` when the
     *        identifier is stale.
     */
    [[nodiscard]] virtual const IEdge *edge(EdgeId id) const noexcept = 0;

    // ------ Traversal ------

    /**
     * @brief Walks the graph starting at @p startNode under @p mode and
     *        reports each visited vertex and edge to @p visitor.
     *
     * Returns a successful @ref Result on normal completion AND when the
     * visitor requested an early exit via @ref VisitResult::Stop — the
     * stop signal is a normal control-flow outcome, not an error. Returns
     * an error @ref Result only when @ref TraverseMode::Topological
     * encounters a cycle or when an implementation-defined failure (e.g.
     * @p startNode is stale) prevents the walk.
     */
    virtual Result traverse(NodeId startNode, TraverseMode mode, IGraphVisitor &visitor) = 0;

    // ------ Query ------

    /**
     * @brief Returns the read-only query surface.
     *
     * The returned reference is valid for the lifetime of the graph.
     */
    [[nodiscard]] virtual const IGraphQuery &query() const noexcept = 0;

    // ------ Observability ------

    /**
     * @brief Returns the number of live nodes.
     */
    [[nodiscard]] virtual std::size_t nodeCount() const noexcept = 0;

    /**
     * @brief Returns the number of live edges.
     */
    [[nodiscard]] virtual std::size_t edgeCount() const noexcept = 0;

    // ------ Tooling ------

    /**
     * @brief Serialises the current graph to GraphViz DOT into @p out.
     *
     * The implementation writes to the caller-supplied buffer only; it
     * never touches the file system. Existing contents of @p out are
     * overwritten.
     */
    virtual Result exportGraphViz(std::string &out) const = 0;

  protected:
    IGraph() = default;

  public:
    IGraph(const IGraph &)            = delete;
    IGraph &operator=(const IGraph &) = delete;
    IGraph(IGraph &&)                 = delete;
    IGraph &operator=(IGraph &&)      = delete;
};

} // namespace vigine::graph
