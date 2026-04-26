#include "vigine/api/taskflow/abstracttaskflow.h"

#include <memory>
#include <utility>

#include "taskorchestrator.h"
#include "vigine/result.h"
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

    // Execute the runnable. Concrete tasks derive from
    // @ref vigine::AbstractTask which makes @c setApi / @c api final
    // and stores the bound token; the engine wires the token in this
    // call site through @c setApi before @c run and clears it through
    // an RAII guard so a throwing @c run still leaves the task with a
    // null binding.
    //
    // The wrapper does NOT yet thread an aggregator into the flow (UD-3
    // keeps the wrapper substrate-only); the engine-token binding is
    // therefore skipped here and set up by the engine when it migrates
    // the per-state flow registry in a follow-up. Tasks that today use
    // @c api() observe a null token and branch on null per the
    // @c IEngineToken contract.
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

    Result outcome;
    {
        runnable->setApi(nullptr);
        [[maybe_unused]] ApiBindingGuard guard(runnable);
        outcome = runnable->run();
    }

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
