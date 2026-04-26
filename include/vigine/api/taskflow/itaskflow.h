#pragma once

#include <memory>

#include "vigine/result.h"
#include "vigine/api/taskflow/resultcode.h"
#include "vigine/api/taskflow/routemode.h"
#include "vigine/api/taskflow/taskid.h"

namespace vigine
{
class ITask;

/**
 * @brief Pure-virtual forward-declared stub for the legacy task flow
 *        surface.
 *
 * @ref ITaskFlow is a minimal stub whose only contract is a virtual
 * destructor. It exists so that callers that want a stable reference
 * to "some task flow" can hold a pointer to a pure-virtual interface
 * without requiring the legacy @c TaskFlow (declared in
 * @c include/vigine/taskflow.h) to migrate onto the new wrapper
 * surface in this leaf. The richer wrapper surface — task
 * registration, result-code transitions, and so on — lives under
 * @c vigine::taskflow::ITaskFlow in the nested namespace block below;
 * a later leaf moves the legacy @c TaskFlow onto that surface and
 * retires this stub.
 *
 * Ownership: this type is never instantiated directly. The concrete
 * legacy @c TaskFlow object derives from it when that migration
 * eventually lands; for now the stub carries no users on its own.
 */
class ITaskFlow
{
  public:
    virtual ~ITaskFlow() = default;

    ITaskFlow(const ITaskFlow &)            = delete;
    ITaskFlow &operator=(const ITaskFlow &) = delete;
    ITaskFlow(ITaskFlow &&)                 = delete;
    ITaskFlow &operator=(ITaskFlow &&)      = delete;

  protected:
    ITaskFlow() = default;
};

} // namespace vigine

namespace vigine::taskflow
{
/**
 * @brief Pure-virtual Level-1 wrapper surface for the task flow.
 *
 * @ref ITaskFlow is the user-facing contract over the task flow
 * substrate: it registers tasks, wires result-code-driven transitions
 * between them, picks the starting task, advances the flow, and
 * reports which task is currently active. The interface knows nothing
 * about the underlying graph storage; substrate primitive types never
 * appear in the public API per INV-11. The stateful base
 * @ref AbstractTaskFlow carries an opaque internal task orchestrator
 * through a private @c std::unique_ptr so the substrate stays hidden
 * from consumers of this header.
 *
 * Ownership and lifetime:
 *   - Concrete task flows are constructed through the non-template
 *     factory in @ref factory.h and handed back as
 *     @c std::unique_ptr<ITaskFlow>. The caller owns the returned
 *     pointer.
 *   - Tasks are opaque slots addressed by @ref TaskId value handles;
 *     the flow never hands out raw pointers to its internal task
 *     slots. Registering a task yields its generational @ref TaskId;
 *     the handle stays stable until the flow is destroyed. User
 *     logic for the task is not part of this leaf — tasks are plain
 *     containers whose behaviour is wired through a later leaf that
 *     attaches work callbacks to @ref TaskId handles.
 *
 * Routing model (UD-3):
 *   - Sync transitions fire when a completed task reports a
 *     @ref ResultCode. The flow looks up the list of next tasks
 *     registered via @ref onResult against the @c (source, resultCode)
 *     pair and advances according to the transition's @ref RouteMode.
 *   - @ref RouteMode::FirstMatch is the default. Callers pass
 *     @ref RouteMode::FanOut to register a transition that lets every
 *     next task registered for the same pair run, or
 *     @ref RouteMode::Chain to register a transition that pipelines
 *     through every registered target sequentially.
 *   - Async routing via signals lands in a later leaf that wires the
 *     flow to the messaging bus. This leaf only models the sync path
 *     so the wrapper primitive can be exercised in isolation.
 *
 * Thread-safety: the contract does not fix one. The default
 * implementation inherits the substrate's reader-writer policy; the
 * concrete flow exposed through @c createTaskFlow serialises
 * mutations with the same @c std::shared_mutex the underlying graph
 * uses. Concurrent queries are safe with each other; concurrent
 * mutations take the exclusive lock.
 *
 * INV-1 compliance: the surface uses no template parameters. INV-10
 * compliance: the name carries the @c I prefix for a pure-virtual
 * interface. INV-11 compliance: the public API mentions only task
 * flow domain handles (@ref TaskId, @ref ResultCode, @ref RouteMode,
 * @ref Result); no graph primitive types cross the boundary.
 */
class ITaskFlow
{
  public:
    virtual ~ITaskFlow() = default;

    // ------ Task registration ------

    /**
     * @brief Allocates a fresh task slot and returns its generational
     *        handle.
     *
     * The returned handle is always valid. Every subsequent query
     * (@ref hasTask, @ref current) and every transition registration
     * accepts the returned id; recycled slots carry a bumped
     * generation so stale handles fail safely.
     */
    [[nodiscard]] virtual TaskId addTask() = 0;

    /**
     * @brief Reports whether the task flow currently tracks the task
     *        addressed by @p task.
     *
     * Useful for pre-flight checks in callers that want to skip
     * silently rather than error out when a task has been removed by
     * another path between ticks.
     */
    [[nodiscard]] virtual bool hasTask(TaskId task) const noexcept = 0;

    // ------ Transitions ------

    /**
     * @brief Registers a transition: when @p source completes with
     *        @p code the flow advances to @p next.
     *
     * Shorthand for @ref onResult with @ref RouteMode::FirstMatch —
     * the UD-3 default. Multiple calls with the same
     * @c (source, code) pair stack up in registration order; the
     * default @c FirstMatch mode routes to the first registered
     * target and ignores the rest.
     *
     * Both @p source and @p next must have been registered via
     * @ref addTask beforehand. Reports @ref Result::Code::Error when
     * either id is stale or when the source id is the same as the
     * next id (a task cannot transition directly to itself through
     * a zero-length transition — callers that need a loop wire it
     * through an explicit intermediate task).
     */
    virtual Result onResult(TaskId source, ResultCode code, TaskId next) = 0;

    /**
     * @brief Registers a transition with an explicit routing mode.
     *
     * Use this overload to opt into @ref RouteMode::FanOut (every
     * target registered against the pair runs independently) or
     * @ref RouteMode::Chain (registered targets run sequentially as
     * a pipeline). Multiple registrations with the same
     * @c (source, code) pair must agree on the @ref RouteMode; the
     * wrapper reports @ref Result::Code::Error on a conflicting
     * re-registration so the routing contract stays unambiguous.
     *
     * Both @p source and @p next must have been registered via
     * @ref addTask beforehand. Reports @ref Result::Code::Error when
     * either id is stale, when the source id is the same as the next
     * id, or when the re-registration conflicts with the already-
     * stored @ref RouteMode for that pair.
     */
    virtual Result onResult(
        TaskId    source,
        ResultCode code,
        TaskId    next,
        RouteMode mode) = 0;

    // ------ Runnable attachment ------

    /**
     * @brief Binds a runnable @ref vigine::ITask to the slot identified
     *        by @p taskId so @ref runCurrentTask invokes it when the
     *        flow's cursor reaches that task.
     *
     * The flow takes ownership of @p task. A successful registration is
     * one-shot per slot: callers that need to swap a runnable on an
     * existing task rebuild the flow from scratch. The runnable lives
     * until the flow is destroyed; there is no detach surface in this
     * leaf.
     *
     * Reports @ref Result::Code::Error when @p taskId is stale (the
     * orchestrator never registered or has retired it) or when @p task
     * is @c nullptr. Reports @ref Result::Code::Error when a runnable
     * was already bound to @p taskId; ownership of @p task is consumed
     * regardless of result, matching the @ref addStateTaskFlow contract
     * on @ref vigine::statemachine::IStateMachine — the parameter is
     * taken by value, so the @c std::unique_ptr held by the parameter
     * is destroyed at function return on every failure path and the
     * runnable is released along with it.
     *
     * Threading: callers serialise @c attachTaskRun against any
     * @ref runCurrentTask path on the same flow externally; the wrapper
     * does not introduce a per-flow mutex on the runnable registry
     * because the typical wiring loads every runnable during topology
     * setup before the engine pump starts.
     */
    virtual Result attachTaskRun(TaskId taskId, std::unique_ptr<vigine::ITask> task) = 0;

    /**
     * @brief Executes the runnable bound to the current task slot and
     *        advances the cursor to whichever next task the registered
     *        transitions select.
     *
     * @ref vigine::engine::IEngine::run drives every pump tick along
     * the following shape:
     *
     *   1. Look up the runnable bound to @ref current. When no runnable
     *      is attached the call is a no-op and the cursor clears so
     *      @ref hasTasksToRun reports false on the next probe.
     *   2. Invoke the runnable's @c run(). The returned @ref Result is
     *      mapped to the closest @ref ResultCode (Success / Error) and
     *      the orchestrator looks up the @c onResult transition for
     *      the @c (current, code) pair.
     *   3. When a matching transition exists the cursor advances to
     *      the next task. When no transition is registered the cursor
     *      clears and @ref hasTasksToRun reports false on the next
     *      probe — the engine pump then falls through to the FSM drain
     *      alone.
     *
     * The engine wires a state-scoped @c IEngineToken into the
     * runnable through @ref vigine::ITask::setApi before invoking
     * @c run and clears the binding through an RAII guard so a
     * throwing @c run still leaves the task with a null token; that
     * sequencing is required by the R-StateScope contract documented
     * on @ref vigine::ITask.
     *
     * Threading: not thread-safe. The engine pump calls this from the
     * controller thread; callers that drive their own pump must
     * serialise externally.
     */
    virtual void runCurrentTask() = 0;

    /**
     * @brief Reports whether the flow has more work to drive.
     *
     * Returns @c true when @ref current names a task slot with an
     * attached runnable that has not yet been consumed — i.e. the next
     * @ref runCurrentTask invocation will execute a runnable. Returns
     * @c false when the cursor has been cleared (the previous
     * @ref runCurrentTask returned a @ref ResultCode that has no
     * transition wired) or when no runnable is attached to the current
     * task slot.
     *
     * The engine pump probes this every tick and skips the
     * @ref runCurrentTask call when it returns @c false, falling
     * through to the FSM drain + main-thread pump alone.
     */
    [[nodiscard]] virtual bool hasTasksToRun() const noexcept = 0;

    // ------ Flow control ------

    /**
     * @brief Marks @p start as the task the flow begins with.
     *
     * The referenced task must have been registered through
     * @ref addTask. Reports @ref Result::Code::Error when @p start is
     * stale; on success the next @ref current call returns @p start.
     *
     * The flow auto-provisions a default start task in its
     * constructor per UD-3, so callers that never register their own
     * tasks still observe a valid @ref current id. Callers that
     * register their own tasks freely override the selection with
     * this call before they begin driving the flow.
     */
    virtual Result enqueue(TaskId start) = 0;

    /**
     * @brief Returns the task the flow currently considers active.
     *
     * Every concrete flow reports a valid id after construction
     * because the constructor auto-provisions the default task per
     * UD-3. A caller that never registers its own tasks still sees
     * the default task as the current one.
     */
    [[nodiscard]] virtual TaskId current() const noexcept = 0;

    ITaskFlow(const ITaskFlow &)            = delete;
    ITaskFlow &operator=(const ITaskFlow &) = delete;
    ITaskFlow(ITaskFlow &&)                 = delete;
    ITaskFlow &operator=(ITaskFlow &&)      = delete;

  protected:
    ITaskFlow() = default;
};

} // namespace vigine::taskflow
