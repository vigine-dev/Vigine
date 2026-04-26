#include "vigine/api/taskflow/abstracttaskflow.h"

#include <memory>
#include <utility>

#include "taskorchestrator.h"
#include "vigine/result.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/messaging/isignalemitter.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
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
// R-StateScope binding shape (setApiToken -> run -> setApiToken(nullptr) under an
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

TaskId AbstractTaskFlow::addTask(std::unique_ptr<vigine::ITask> task)
{
    /*
     * Convenience overload: allocate a slot AND bind the runnable in
     * a single call. Reject a null @p task before allocating so we do
     * not leave an orphan task id behind on misuse. After the slot is
     * reserved we delegate to attachTaskRun for the binding so the
     * storage and error-handling stays in one place.
     */
    if (task == nullptr)
    {
        return TaskId{};
    }
    const TaskId taskId = addTask();
    if (!taskId.valid())
    {
        return taskId;
    }
    if (!attachTaskRun(taskId, std::move(task)).isSuccess())
    {
        return TaskId{};
    }
    return taskId;
}

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
     * @c setApiToken / @c api final and stores the bound token; the flow
     * binds the long-lived per-state token onto the runnable through
     * @c setApiToken before @c run and clears the binding back to nullptr
     * through an RAII guard so a throwing @c run still leaves the task
     * with a null apiToken() once the call returns.
     *
     * Per-state token lifetime. The token bound on every tick is the
     * one stored in @ref _activeToken — minted in @ref setActiveState
     * when the engine pump tells the flow which FSM state it is in.
     * The token persists across ticks for as long as the state is
     * active, so a task that subscribes to expiration in one tick can
     * legally retain its @ref vigine::engine::IEngineToken::ExpirationToken
     * across subsequent ticks. The token is destroyed only when the
     * active state changes (the next @ref setActiveState call drops
     * the old token, firing every subscriber's callback) or when the
     * flow itself is destroyed.
     *
     * No-context / no-state fallback. When @ref setContext has never
     * been called or @ref setActiveState has not been driven, the
     * @ref _activeToken stays null. @ref runCurrentTask then binds
     * @c nullptr onto the runnable and tasks observe apiToken() == nullptr
     * inside @c run(). Tests that drive the flow directly without the
     * engine pump rely on this shape.
     */
    struct ApiBindingGuard
    {
        vigine::ITask *task;
        explicit ApiBindingGuard(vigine::ITask *t) : task(t) {}
        ~ApiBindingGuard()
        {
            if (task != nullptr)
            {
                task->setApiToken(nullptr);
            }
        }
        ApiBindingGuard(const ApiBindingGuard &)            = delete;
        ApiBindingGuard &operator=(const ApiBindingGuard &) = delete;
        ApiBindingGuard(ApiBindingGuard &&)                 = delete;
        ApiBindingGuard &operator=(ApiBindingGuard &&)      = delete;
    };

    Result outcome;
    {
        runnable->setApiToken(_activeToken.get());
        [[maybe_unused]] ApiBindingGuard guard(runnable);
        outcome = runnable->run();
    }
    /*
     * On scope exit ApiBindingGuard fires setApiToken(nullptr); the
     * per-state @ref _activeToken stays alive for subsequent ticks
     * until the next @ref setActiveState call drops it.
     */

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
     * pointer to the IContext that mints per-state engine tokens; the
     * engine pump installs the pointer before each tick through
     * AbstractEngine::run. A nullptr argument detaches the binding so
     * subsequent setActiveState calls fall back to the no-token shape —
     * useful for tests that drive the flow directly bypassing the
     * engine pump and that want to observe apiToken() == nullptr inside run().
     */
    _context = context;
}

void AbstractTaskFlow::setSignalEmitter(vigine::messaging::ISignalEmitter *emitter) noexcept
{
    /*
     * Plain raw-pointer assignment. The flow stores a non-owning back-
     * pointer to the signal emitter wired by the host (typically the
     * application's main()). A nullptr argument detaches the binding so
     * subsequent signal() calls report Result::Code::Error until a
     * fresh emitter lands.
     */
    _signalEmitter = emitter;
}

Result AbstractTaskFlow::signal(TaskId                                  source,
                                TaskId                                  target,
                                vigine::payload::PayloadTypeId        payloadTypeId,
                                vigine::core::threading::ThreadAffinity affinity)
{
    /*
     * Flow-level signal subscription. The flow owns the subscription
     * token returned by ISignalEmitter::subscribeSignal so callers do
     * not have to maintain an external sink for the subscription's
     * lifetime. Tokens unwind at flow destruction in the reverse-order
     * pass over @ref _signalSubscriptions, which is declared AFTER
     * @ref _runnables so subscriptions cancel BEFORE the underlying
     * subscriber objects (the runnables themselves) are freed.
     *
     * The @p affinity parameter is kept for the documented per-
     * subscription dispatch hint; today the actual delivery thread is
     * fixed by the emitter's bus configuration. The signature reserves
     * the slot for a future emitter overload that takes an explicit
     * per-subscription override without breaking the public surface.
     */
    static_cast<void>(affinity);

    if (_signalEmitter == nullptr)
    {
        return Result(Result::Code::Error,
                      "signal: no signal emitter wired (call setSignalEmitter first)");
    }
    if (!_orchestrator->hasTask(source))
    {
        return Result(Result::Code::Error,
                      "signal: source task is not registered");
    }
    if (!_orchestrator->hasTask(target))
    {
        return Result(Result::Code::Error,
                      "signal: target task is not registered");
    }

    auto it = _runnables.find(target);
    if (it == _runnables.end() || it->second == nullptr)
    {
        return Result(Result::Code::Error,
                      "signal: target task has no runnable attached");
    }

    auto *subscriber =
        dynamic_cast<vigine::messaging::ISubscriber *>(it->second.get());
    if (subscriber == nullptr)
    {
        return Result(Result::Code::Error,
                      "signal: target runnable does not implement ISubscriber");
    }

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = payloadTypeId;

    auto token = _signalEmitter->subscribeSignal(filter, subscriber);
    if (!token)
    {
        return Result(Result::Code::Error,
                      "signal: emitter subscribeSignal returned null");
    }

    _signalSubscriptions.push_back(std::move(token));
    return Result();
}

void AbstractTaskFlow::setActiveState(vigine::statemachine::StateId state) noexcept
{
    /*
     * Idempotent on a same-state call: the engine pump invokes this
     * every tick, so most calls are no-ops. When the state genuinely
     * changes, drop the existing per-state token first — its destructor
     * fires the engine-token expiration callbacks for every task that
     * subscribed during the prior state, satisfying the R-StateScope
     * "old state's token expires precisely on transition out" contract.
     * Then mint a fresh token bound to the new state when a context is
     * wired; without one we leave _activeToken null and tasks observe
     * apiToken() == nullptr until the engine pump installs a context.
     */
    if (_activeState == state)
    {
        return;
    }

    _activeState = state;
    _activeToken.reset();
    if (_context != nullptr)
    {
        _activeToken = _context->makeEngineToken(state);
    }
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

Result AbstractTaskFlow::setRoot(TaskId root)
{
    if (!_orchestrator->hasTask(root))
    {
        return Result(Result::Code::Error, "setRoot: root task is not registered");
    }
    _current = root;
    return Result();
}

TaskId AbstractTaskFlow::current() const noexcept
{
    return _current;
}

} // namespace vigine::taskflow
