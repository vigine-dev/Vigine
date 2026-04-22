#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "vigine/graph/abstractgraph.h"
#include "vigine/graph/nodeid.h"
#include "vigine/result.h"
#include "vigine/statemachine/stateid.h"

namespace vigine::statemachine
{
/**
 * @brief Internal graph specialisation that the state machine wrapper
 *        uses to hold its state-and-hierarchy storage.
 *
 * @ref StateTopology is a concrete @c vigine::graph::AbstractGraph
 * subtype that seals the inheritance chain for the state machine
 * wrapper. It carries the translation between the wrapper's own POD
 * handle (@ref StateId) and the substrate's generational @c NodeId
 * so every @ref IStateMachine method can delegate a single call to
 * it without letting substrate primitives leak through the public
 * wrapper surface.
 *
 * This header lives under @c src/statemachine on purpose: the INV-11
 * rule forbids @c vigine::graph types from surfacing in
 * @c include/vigine/statemachine. Only the wrapper implementation
 * consumes the topology; callers of @ref IStateMachine /
 * @ref AbstractStateMachine see neither the topology nor its graph
 * base.
 *
 * Thread-safety inherits from @c AbstractGraph: every mutating entry
 * point takes the graph's exclusive lock; reads take a shared lock.
 * The wrapper layer does not add any additional synchronisation on
 * top; every state-machine-side access path funnels through the
 * topology.
 *
 * Node kinds used:
 *   - @c vigine::fsm::kind::State for state nodes (from
 *     @c include/vigine/fsm/kind.h).
 *
 * Edge kinds used:
 *   - @c vigine::fsm::edge_kind::ChildOf for the directed edge that
 *     ties a child state back to its parent (bubble traversal).
 *   - @c vigine::fsm::edge_kind::Transition is reserved for a later
 *     leaf that wires the machine to the message bus; this leaf
 *     only maintains the hierarchy.
 */
class StateTopology final : public vigine::graph::AbstractGraph
{
  public:
    StateTopology();
    ~StateTopology() override;

    StateTopology(const StateTopology &)            = delete;
    StateTopology &operator=(const StateTopology &) = delete;
    StateTopology(StateTopology &&)                 = delete;
    StateTopology &operator=(StateTopology &&)      = delete;

    // ------ State lifecycle ------

    /**
     * @brief Allocates a fresh state node and returns the
     *        corresponding @ref StateId.
     *
     * The returned handle is always valid; the underlying graph
     * never reports a generation of zero from
     * @ref AbstractGraph::addNode.
     */
    [[nodiscard]] StateId addState();

    /**
     * @brief Reports whether a state node addressed by @p state is
     *        currently tracked.
     */
    [[nodiscard]] bool hasState(StateId state) const noexcept;

    // ------ Hierarchy ------

    /**
     * @brief Creates a directed @c ChildOf edge from @p child to
     *        @p parent.
     *
     * Reports @ref Result::Code::Error when either state is stale,
     * when the pair is identical, or when the edge would introduce
     * a cycle (i.e. @p child is already an ancestor of @p parent).
     * The edge direction is child->parent because the natural query
     * at dispatch time is "who is the parent of the current state?"
     * and that matches @c outEdgesOfKind on the child.
     */
    Result addChildEdge(StateId parent, StateId child);

    /**
     * @brief Returns the parent of @p state, or a default-constructed
     *        invalid @ref StateId when @p state is a root.
     *
     * A state with more than one outgoing @c ChildOf edge is ill-
     * formed (the wrapper rejects multi-parent registrations in
     * @ref addChildEdge), so the walk only needs to look at the
     * first matching edge.
     */
    [[nodiscard]] StateId parentOf(StateId state) const;

    /**
     * @brief Returns @c true when @p ancestor sits on the parent
     *        chain of @p descendant.
     *
     * The relation is strict: a state is never its own ancestor. An
     * invalid @p ancestor or @p descendant always returns @c false.
     * The walk depth is capped by @ref kMaxHierarchyDepth as a
     * defensive safeguard against accidental cycles; the cap is far
     * above any realistic HSM depth.
     */
    [[nodiscard]] bool isAncestorOf(StateId ancestor, StateId descendant) const;

    // ------ POD translation helpers ------

    /**
     * @brief Translates a @ref StateId to the substrate's
     *        @c NodeId.
     *
     * Packaged as a free-standing @c static helper so the wrapper
     * implementation can reach the substrate without needing further
     * access to the graph's internals. The two POD types have the
     * same layout; the helper exists for type-safety, not for
     * arithmetic.
     */
    [[nodiscard]] static vigine::graph::NodeId toNodeId(StateId state) noexcept;

    /**
     * @brief Translates a substrate @c NodeId back to a
     *        @ref StateId.
     *
     * Only the wrapper implementation calls this; callers of the
     * public state machine API never see the substrate type.
     */
    [[nodiscard]] static StateId toStateId(vigine::graph::NodeId node) noexcept;

  private:
    /**
     * @brief Serialises the probe+add sequence inside
     *        @ref addChildEdge.
     *
     * `addChildEdge` runs three separate graph operations (ancestor
     * check, existing-parent probe, edge insertion). The underlying
     * graph takes an exclusive lock per call, but nothing stops two
     * racing callers from both seeing "no existing parent" and
     * both inserting a `ChildOf` edge — violating the single-parent
     * invariant the wrapper advertises. This mutex groups the three
     * operations into one atomic critical section on the wrapper
     * side.
     */
    mutable std::mutex _hierarchyMutex;

    /**
     * @brief Maximum depth the ancestor walk traverses before
     *        bailing out.
     *
     * The cap protects against accidental cycles the wrapper's
     * @ref addChildEdge validation somehow missed. Any realistic HSM
     * sits far below this depth; the cap mostly exists so a bug in
     * @ref addChildEdge cannot hang the caller.
     */
    static constexpr std::size_t kMaxHierarchyDepth = 1024;
};

} // namespace vigine::statemachine
