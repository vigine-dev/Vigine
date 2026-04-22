#include "vigine/taskflow/abstracttaskflow.h"

#include <memory>

#include "taskflow/taskorchestrator.h"
#include "vigine/result.h"
#include "vigine/taskflow/resultcode.h"
#include "vigine/taskflow/routemode.h"
#include "vigine/taskflow/taskid.h"

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
