#pragma once

#include <cstddef>
#include <cstdint>

#include "vigine/core/graph/abstractgraph.h"
#include "vigine/core/graph/edgeid.h"
#include "vigine/core/graph/nodeid.h"
#include "vigine/result.h"
#include "vigine/taskflow/resultcode.h"
#include "vigine/taskflow/routemode.h"
#include "vigine/taskflow/taskid.h"

namespace vigine::taskflow
{
/**
 * @brief Internal graph specialisation that the task flow wrapper uses
 *        to hold its tasks and their transitions.
 *
 * @ref TaskOrchestrator is a concrete @c vigine::core::graph::AbstractGraph
 * subtype that seals the inheritance chain for the task flow wrapper.
 * It carries the translation between the wrapper's own POD handle
 * (@ref TaskId) and the substrate's generational @c NodeId so every
 * @ref ITaskFlow method can delegate a single call to it without
 * letting substrate primitives leak through the public wrapper
 * surface.
 *
 * This header lives under @c src/taskflow on purpose: the INV-11 rule
 * forbids @c vigine::core::graph types from surfacing in
 * @c include/vigine/taskflow. Only the wrapper implementation
 * consumes the orchestrator; callers of @ref ITaskFlow /
 * @ref AbstractTaskFlow see neither the orchestrator nor its graph
 * base.
 *
 * Thread-safety inherits from @c AbstractGraph: every mutating entry
 * point takes the graph's exclusive lock; reads take a shared lock.
 * The wrapper layer does not add any additional synchronisation on
 * top; every task-flow-side access path funnels through the
 * orchestrator.
 *
 * Node kinds used:
 *   - @c vigine::taskflow::kind::Task for task nodes (from
 *     @c include/vigine/taskflow/kind.h).
 *
 * Edge kinds used:
 *   - @c vigine::taskflow::edge_kind::Transition for the directed
 *     edge from a source task to its registered next task. The edge
 *     carries the @ref ResultCode that triggers the transition and
 *     the @ref RouteMode that determines how the wrapper walks the
 *     edges registered against the same
 *     @c (source, resultCode) pair.
 */
class TaskOrchestrator final : public vigine::core::graph::AbstractGraph
{
  public:
    TaskOrchestrator();
    ~TaskOrchestrator() override;

    TaskOrchestrator(const TaskOrchestrator &)            = delete;
    TaskOrchestrator &operator=(const TaskOrchestrator &) = delete;
    TaskOrchestrator(TaskOrchestrator &&)                 = delete;
    TaskOrchestrator &operator=(TaskOrchestrator &&)      = delete;

    // ------ Task lifecycle ------

    /**
     * @brief Allocates a fresh task node and returns the corresponding
     *        @ref TaskId.
     *
     * The returned handle is always valid; the underlying graph never
     * returns an invalid generation from
     * @ref vigine::core::graph::AbstractGraph::addNode.
     */
    [[nodiscard]] TaskId addTask();

    /**
     * @brief Reports whether a task node addressed by @p task is
     *        currently tracked.
     */
    [[nodiscard]] bool hasTask(TaskId task) const noexcept;

    // ------ Transitions ------

    /**
     * @brief Creates a directed transition edge from @p source to
     *        @p next triggered by @p code and routed with @p mode.
     *
     * Reports @ref Result::Code::Error when either task is stale,
     * when @p source equals @p next (a task cannot transition
     * directly to itself), or when a conflicting @ref RouteMode has
     * already been stored for the @c (source, code) pair — the first
     * registration locks the routing mode for the pair.
     */
    Result addTransition(
        TaskId    source,
        ResultCode code,
        TaskId    next,
        RouteMode mode);

    // ------ POD translation helpers ------

    /**
     * @brief Translates a @ref TaskId to the substrate's
     *        @c NodeId.
     *
     * Packaged as a free-standing @c static helper so the wrapper
     * implementation can reach the substrate without needing further
     * access to the graph's internals. The two POD types have the
     * same layout; the helper exists for type-safety, not for
     * arithmetic.
     */
    [[nodiscard]] static vigine::core::graph::NodeId toNodeId(TaskId task) noexcept;

    /**
     * @brief Translates a substrate @c NodeId back to a
     *        @ref TaskId.
     *
     * Only the wrapper implementation calls this; callers of the
     * public task flow API never see the substrate type.
     */
    [[nodiscard]] static TaskId toTaskId(vigine::core::graph::NodeId node) noexcept;

  private:
    /**
     * @brief Reports the @ref RouteMode already stored for the
     *        @c (source, code) pair, or the default @p fallback
     *        when no edge is yet registered.
     *
     * Used by @ref addTransition to enforce the "one @ref RouteMode
     * per pair" invariant: the first registration fixes the mode;
     * every subsequent registration for the same pair must repeat
     * the same mode.
     */
    [[nodiscard]] RouteMode storedModeFor(
        TaskId     source,
        ResultCode code,
        RouteMode  fallback) const;
};

} // namespace vigine::taskflow
