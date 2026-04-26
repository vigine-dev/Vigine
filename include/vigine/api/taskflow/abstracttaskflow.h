#pragma once

#include <memory>
#include <unordered_map>

#include "vigine/result.h"
#include "vigine/api/taskflow/itaskflow.h"
#include "vigine/api/taskflow/resultcode.h"
#include "vigine/api/taskflow/routemode.h"
#include "vigine/api/taskflow/taskid.h"

namespace vigine
{
class ITask;
} // namespace vigine

namespace vigine::taskflow
{
// Forward declaration only. The concrete TaskOrchestrator type is a
// substrate-primitive specialisation defined under @c src/taskflow and
// is never exposed in the public header tree — see INV-11, wrapper
// encapsulation.
class TaskOrchestrator;

/**
 * @brief Stateful abstract base that every concrete task flow derives
 *        from.
 *
 * @ref AbstractTaskFlow is level 4 of the wrapper recipe the engine's
 * Level-1 subsystem wrappers follow. It carries the state every
 * concrete task flow shares — a private handle to the internal task
 * orchestrator and the currently active @ref TaskId — and supplies
 * default implementations of every @ref ITaskFlow method so a minimal
 * concrete task flow only needs to seal the inheritance chain. The
 * internal task orchestrator specialises the graph substrate and
 * translates between @ref TaskId and the substrate's own identifier
 * types inside its implementation.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. The
 * base is abstract in the logical sense; its default constructor
 * wires up a fresh internal task orchestrator and auto-provisions the
 * default task per UD-3 so every concrete task flow has a live
 * substrate and a valid @ref current task as soon as it is
 * constructed.
 *
 * Composition, not inheritance:
 *   - @ref AbstractTaskFlow HAS-A private
 *     @c std::unique_ptr<TaskOrchestrator>. It does @b not inherit
 *     from the substrate primitive at the wrapper level. The
 *     internal task orchestrator is the only place where substrate
 *     primitives enter the task flow stack, and it lives strictly
 *     under @c src/taskflow. This keeps the public header tree free
 *     of substrate types (INV-11) and makes the "a task flow IS NOT
 *     a substrate graph" relationship explicit.
 *
 * Default-task auto-provisioning (UD-3):
 *   - The constructor registers one default task and selects it as
 *     the start (and current) task. A caller that never registers
 *     its own tasks still sees a valid @ref current id. A caller
 *     that registers its own tasks freely overrides the selection
 *     with @ref enqueue.
 *   - The default task id is stored as a private member so the
 *     concrete closer can expose it through an internal helper if it
 *     ever needs to (the public API does not surface it separately).
 *
 * Routing (UD-3):
 *   - The default @ref RouteMode for transitions registered through
 *     the short overload of @ref onResult is @ref RouteMode::FirstMatch,
 *     matching the back-compat behaviour of the legacy task flow.
 *     Callers opt into @ref RouteMode::FanOut or @ref RouteMode::Chain
 *     per transition through the explicit-mode overload.
 *   - Per @c (source, ResultCode) pair the orchestrator stores a
 *     single @ref RouteMode; re-registrations with a conflicting
 *     mode report @ref Result::Code::Error so callers cannot
 *     accidentally mix routing semantics for the same pair.
 *
 * Strict encapsulation:
 *   - All data members are @c private. Derived task flow classes
 *     reach internal state through @c protected accessors; the
 *     single getter returns a reference to the task orchestrator so
 *     concrete derivatives can extend the default implementation
 *     without re-exporting the substrate on their own public surface.
 *
 * Thread-safety: the base inherits the task orchestrator's
 * thread-safety policy (reader-writer mutex on the substrate
 * primitive). Callers may query and mutate concurrently; each
 * mutation takes the exclusive lock while each query takes a shared
 * lock. The wrapper layer does not add further synchronisation —
 * every task-flow access path funnels through the orchestrator.
 */
class AbstractTaskFlow : public ITaskFlow
{
  public:
    ~AbstractTaskFlow() override;

    // ------ ITaskFlow: task registration ------

    [[nodiscard]] TaskId addTask() override;
    [[nodiscard]] bool   hasTask(TaskId task) const noexcept override;

    // ------ ITaskFlow: transitions ------

    Result onResult(TaskId source, ResultCode code, TaskId next) override;
    Result onResult(
        TaskId    source,
        ResultCode code,
        TaskId    next,
        RouteMode mode) override;

    // ------ ITaskFlow: runnable attachment ------

    Result               attachTaskRun(TaskId taskId,
                                       std::unique_ptr<vigine::ITask> task) override;
    void                 runCurrentTask() override;
    [[nodiscard]] bool   hasTasksToRun() const noexcept override;

    // ------ ITaskFlow: flow control ------

    Result               enqueue(TaskId start) override;
    [[nodiscard]] TaskId current() const noexcept override;

    AbstractTaskFlow(const AbstractTaskFlow &)            = delete;
    AbstractTaskFlow &operator=(const AbstractTaskFlow &) = delete;
    AbstractTaskFlow(AbstractTaskFlow &&)                 = delete;
    AbstractTaskFlow &operator=(AbstractTaskFlow &&)      = delete;

  protected:
    AbstractTaskFlow();

    /**
     * @brief Returns a mutable reference to the internal task
     *        orchestrator.
     *
     * Exposed as @c protected so that follow-up concrete task flow
     * classes can add their own specialised cache or traversal on top
     * of the default implementation without re-exporting the
     * substrate on the public surface. The reference is stable for
     * the lifetime of the @ref AbstractTaskFlow instance.
     */
    [[nodiscard]] TaskOrchestrator       &orchestrator() noexcept;
    [[nodiscard]] const TaskOrchestrator &orchestrator() const noexcept;

    /**
     * @brief Returns the id of the default task registered during
     *        construction.
     *
     * Exposed as @c protected so derived closers that want to query
     * or expose the default task (e.g. for diagnostics) can reach it
     * without walking the orchestrator themselves. The public API
     * does not surface the default id separately because callers who
     * register their own tasks use @ref enqueue instead.
     */
    [[nodiscard]] TaskId defaultTask() const noexcept;

  private:
    /**
     * @brief Owns the internal task orchestrator.
     *
     * The orchestrator is a substrate-primitive specialisation
     * defined under @c src/taskflow; forward-declaring it here keeps
     * the substrate out of the public header tree. Held through a
     * @c std::unique_ptr so the orchestrator's full definition does
     * not have to leak through this header.
     */
    std::unique_ptr<TaskOrchestrator> _orchestrator;

    /**
     * @brief Id of the default task registered during construction.
     *
     * Stored so the base can report it through @ref defaultTask
     * without re-querying the orchestrator. Never invalid after
     * construction completes; cleared only when the flow is
     * destroyed.
     */
    TaskId _defaultTask{};

    /**
     * @brief Id of the currently active task.
     *
     * Initialised to @ref _defaultTask during construction so the
     * flow has a valid @ref current immediately. Updated by
     * @ref enqueue.
     */
    TaskId _current{};

    /**
     * @brief Hasher for @ref TaskId so the runnable registry can use
     *        @c std::unordered_map.
     *
     * @ref TaskId is an 8-byte trivially-copyable pair of
     * @c std::uint32_t fields; the hasher splices the index and
     * generation into a single 64-bit value before delegating to the
     * standard library's @c std::hash<std::uint64_t>. Declared inside
     * the private section so the symbol stays scoped to this header
     * and no namespace-level @c std::hash specialisation leaks
     * through the public header tree.
     */
    struct TaskIdHasher
    {
        [[nodiscard]] std::size_t operator()(const TaskId &task) const noexcept
        {
            const std::uint64_t blended =
                (static_cast<std::uint64_t>(task.generation) << 32u)
                | static_cast<std::uint64_t>(task.index);
            return std::hash<std::uint64_t>{}(blended);
        }
    };

    /**
     * @brief Per-task runnable registry populated through
     *        @ref attachTaskRun.
     *
     * Each entry binds a runnable @ref vigine::ITask to a
     * @ref TaskId slot the orchestrator already tracks. The map
     * owns each runnable through @c std::unique_ptr; destroying the
     * flow tears down every attached runnable in turn. Lookups are
     * by @ref TaskId; @ref runCurrentTask uses the lookup to find
     * the runnable for @ref _current and to short-circuit cleanly
     * when the slot is empty.
     *
     * Runnable lifetime: append-only. There is no detach surface in
     * this leaf — once a runnable is attached it lives until the
     * flow is destroyed. Callers that need to swap a runnable
     * rebuild the flow from scratch, matching the
     * @ref vigine::statemachine::IStateMachine::addStateTaskFlow
     * one-shot-per-state contract.
     */
    std::unordered_map<TaskId, std::unique_ptr<vigine::ITask>, TaskIdHasher>
        _runnables;
};

} // namespace vigine::taskflow
