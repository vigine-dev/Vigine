#include "vigine/api/taskflow/abstracttaskflow.h"

#include <memory>
#include <utility>

#include "taskorchestrator.h"
#include "vigine/result.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/api/taskflow/itask.h"
#include "vigine/api/taskflow/resultcode.h"
#include "vigine/api/taskflow/routemode.h"
#include "vigine/api/taskflow/taskid.h"

namespace vigine::taskflow
{

// ---------------------------------------------------------------------------
// Construction / destruction.
//
// The constructor wires up the internal orchestrator, auto-provisions the
// default task per UD-3, and selects it as both the default and the
// current task so @ref current immediately returns a valid id even when
// the caller never registers any task of their own.
// ---------------------------------------------------------------------------

AbstractTaskFlow::AbstractTaskFlow()
    : _orchestrator{std::make_unique<TaskOrchestrator>()}
{
    _defaultTask = _orchestrator->addTask();
    _current     = _defaultTask;
}

AbstractTaskFlow::~AbstractTaskFlow() = default;

// ---------------------------------------------------------------------------
// Protected accessors — the derived classes reach the internal orchestrator
// through these so the substrate stays invisible on the wrapper's
// public surface.
// ---------------------------------------------------------------------------

TaskOrchestrator &AbstractTaskFlow::orchestrator() noexcept
{
    return *_orchestrator;
}

const TaskOrchestrator &AbstractTaskFlow::orchestrator() const noexcept
{
    return *_orchestrator;
}

TaskId AbstractTaskFlow::defaultTask() const noexcept
{
    return _defaultTask;
}

// ---------------------------------------------------------------------------
// ITaskFlow: task registration. Each delegation is a one-liner; the
// orchestrator does the substrate translation so the wrapper stays thin.
// ---------------------------------------------------------------------------

TaskId AbstractTaskFlow::addTask()
{
    return _orchestrator->addTask();
}

bool AbstractTaskFlow::hasTask(TaskId task) const noexcept
{
    return _orchestrator->hasTask(task);
}

// ---------------------------------------------------------------------------
// ITaskFlow: transitions. The short overload routes through the explicit-
// mode overload with the UD-3 default @ref RouteMode::FirstMatch; the
// explicit-mode overload forwards straight to the orchestrator, which
// enforces the "one @ref RouteMode per pair" invariant.
// ---------------------------------------------------------------------------

Result AbstractTaskFlow::onResult(TaskId source, ResultCode code, TaskId next)
{
    return onResult(source, code, next, RouteMode::FirstMatch);
}

Result AbstractTaskFlow::onResult(
    TaskId    source,
    ResultCode code,
    TaskId    next,
    RouteMode mode)
{
    return _orchestrator->addTransition(source, code, next, mode);
}

// ---------------------------------------------------------------------------
// ITaskFlow: runnable attachment. The runnable registry is a small
// per-task map populated by @ref attachTaskRun; @ref runCurrentTask
// looks the runnable for @ref _current up, executes it through the
// R-StateScope binding shape (setApi -> run -> setApi(nullptr) under an
// RAII guard), and advances @c _current through the transition edge
// matching the runnable's reported outcome.
// ---------------------------------------------------------------------------

namespace
{

// Map a generic @ref vigine::Result::Code to the closed
// @ref vigine::taskflow::ResultCode enum the orchestrator stores on
// transition edges. Only the two outcomes that callers can wire today
// (Success / Error) survive the round-trip; Deferred / Skip are
// reserved for the signal-driven facade that lands in a future leaf
// and remain unwired here on purpose.
[[nodiscard]] ResultCode mapResultCode(Result::Code code) noexcept
{
    switch (code)
    {
        case Result::Code::Success:
            return ResultCode::Success;
        default:
            // Every non-Success outcome maps to Error so callers wiring
            // an explicit error route through @c onResult observe the
            // failure path. Tasks that only distinguish Success vs.
            // Error see the closed two-outcome shape the orchestrator
            // stores on transition edges.
            return ResultCode::Error;
    }
}

} // namespace

Result AbstractTaskFlow::attachTaskRun(TaskId taskId,
                                       std::unique_ptr<vigine::ITask> task)
{
    if (task == nullptr)
    {
        return Result(Result::Code::Error,
                      "attachTaskRun: null ITask argument");
    }
    if (!_orchestrator->hasTask(taskId))
    {
        return Result(Result::Code::Error,
                      "attachTaskRun: task id not registered");
    }

    auto [it, inserted] = _runnables.emplace(taskId, std::move(task));
    if (!inserted)
    {
        // One-shot per slot. The parameter has already been moved into
        // the local @p task; on the duplicate-key branch the
        // @c std::unique_ptr held by the parameter is destroyed at
        // function return and the runnable is released along with it.
        // This matches the @ref addStateTaskFlow convention on
        // @ref vigine::statemachine::IStateMachine.
        return Result(Result::Code::Error,
                      "attachTaskRun: task id already has an attached runnable");
    }
    return Result();
}

void AbstractTaskFlow::runCurrentTask()
{
    if (!_current.valid())
    {
        return;
    }

    auto it = _runnables.find(_current);
    if (it == _runnables.end() || it->second == nullptr)
    {
        // Cursor sits on a slot with no runnable attached. Clear the
        // cursor so @ref hasTasksToRun reports false on the next probe
        // and the engine pump falls through to the FSM drain alone.
        _current = TaskId{};
        return;
    }

    vigine::ITask *runnable = it->second.get();

    /*
     * Execute the runnable through the R-StateScope binding shape.
     * Concrete tasks derive from @ref vigine::AbstractTask which makes
     * @c setApi / @c api final and stores the bound token; the flow
     * wires the per-tick token in this call site through @c setApi
     * before @c run and clears it through an RAII guard so a throwing
     * @c run still leaves the task with a null binding.
     *
     * Token-lifetime ordering: the owning @c std::unique_ptr<IEngineToken>
     * is declared OUTSIDE the inner block so that:
     *   1. ApiBindingGuard destructs first on scope exit and runs
     *      setApi(nullptr) — clearing the raw pointer the task may
     *      have read through api().
     *   2. The owning unique_ptr destructs second (after the guard),
     *      destroying the token. The R-StateScope contract requires the
     *      task's bound api() pointer be cleared BEFORE the underlying
     *      token is freed; otherwise a stray callback could observe a
     *      dangling IEngineToken* through api().
     *
     * No-context fallback: when @ref setContext has not been called (or
     * was cleared with @c nullptr), the flow keeps the legacy null-
     * binding shape so tests that drive the flow directly bypassing the
     * engine pump observe api() == nullptr inside run() and branch
     * accordingly.
     *
     * Per-tick state-id binding: the token is minted with a sentinel
     * StateId{} rather than IStateMachine::current(). The aggregator
     * tolerates the sentinel and threads it into the concrete token;
     * gated accessors resolve normally while the bound state matches
     * the sentinel and short-circuit to Code::Expired on transition
     * away. Threading the FSM's live current state into the per-tick
     * mint is a follow-up that lands once IStateMachine exposes its
     * current state into the per-tick path. Per-state TaskFlow scoping
     * itself is already in effect through the engine's
     * taskFlowFor(current()) lookup, so a state transition between
     * ticks switches WHICH flow is pumped without relying on the
     * in-token state-id field.
     */
    struct ApiBindingGuard
    {
        vigine::ITask *task;
        explicit ApiBindingGuard(vigine::ITask *t) : task(t) {}
        ~ApiBindingGuard()
        {
            if (task != nullptr)
            {
                task->setApi(nullptr);
            }
        }
        ApiBindingGuard(const ApiBindingGuard &)            = delete;
        ApiBindingGuard &operator=(const ApiBindingGuard &) = delete;
        ApiBindingGuard(ApiBindingGuard &&)                 = delete;
        ApiBindingGuard &operator=(ApiBindingGuard &&)      = delete;
    };

    std::unique_ptr<vigine::engine::IEngineToken> perTickToken;
    if (_context != nullptr)
    {
        perTickToken = _context->makeEngineToken(
            vigine::statemachine::StateId{});
    }

    Result outcome;
    {
        runnable->setApi(perTickToken.get());
        [[maybe_unused]] ApiBindingGuard guard(runnable);
        outcome = runnable->run();
    }
    // perTickToken destructs here, AFTER the ApiBindingGuard has
    // already cleared the task's bound pointer.

    // Resolve the next task through the orchestrator's transition map.
    // Maps the runnable's @ref Result::Code to the closed
    // @ref ResultCode enum stored on transition edges, then asks the
    // orchestrator for the first matching target. An invalid result
    // means "no transition wired" -- clear the cursor so the next
    // @ref hasTasksToRun probe reports false and the engine pump
    // falls through, matching the legacy completion shape.
    const ResultCode code = mapResultCode(outcome.code());
    const TaskId     next = _orchestrator->nextTaskFor(_current, code);
    _current              = next;
}

void AbstractTaskFlow::setContext(vigine::IContext *context) noexcept
{
    /*
     * Plain raw-pointer assignment. The flow stores a non-owning back-
     * pointer to the IContext that mints per-tick engine tokens; the
     * engine pump installs the pointer before each tick through
     * AbstractEngine::run. A nullptr argument detaches the binding so
     * subsequent runCurrentTask calls fall back to the no-token shape —
     * useful for tests that drive the flow directly bypassing the
     * engine pump and that want to observe api() == nullptr inside run().
     */
    _context = context;
}

bool AbstractTaskFlow::hasTasksToRun() const noexcept
{
    if (!_current.valid())
    {
        return false;
    }
    auto it = _runnables.find(_current);
    return it != _runnables.end() && it->second != nullptr;
}

// ---------------------------------------------------------------------------
// ITaskFlow: flow control. The base enforces that the start task is
// registered before it updates @c _current; callers that pass a stale id
// observe an @ref Result::Code::Error and no state change.
// ---------------------------------------------------------------------------

Result AbstractTaskFlow::enqueue(TaskId start)
{
    if (!_orchestrator->hasTask(start))
    {
        return Result(Result::Code::Error, "start task not registered");
    }
    _current = start;
    return Result();
}

TaskId AbstractTaskFlow::current() const noexcept
{
    return _current;
}

} // namespace vigine::taskflow
