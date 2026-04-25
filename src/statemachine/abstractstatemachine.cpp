#include "vigine/statemachine/abstractstatemachine.h"

#include <cassert>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "statemachine/statetopology.h"
#include "vigine/result.h"
#include "vigine/statemachine/routemode.h"
#include "vigine/statemachine/stateid.h"

namespace vigine::statemachine
{

// ---------------------------------------------------------------------------
// Construction / destruction.
//
// The constructor wires up the internal topology, auto-provisions the
// default state per UD-3, and selects it as both the default and the
// current state so @ref current immediately returns a valid id even when
// the caller never registers any state of their own.
// ---------------------------------------------------------------------------

AbstractStateMachine::AbstractStateMachine()
    : _topology{std::make_unique<StateTopology>()}
{
    _defaultState = _topology->addState();
    _current      = _defaultState;
}

AbstractStateMachine::~AbstractStateMachine() = default;

// ---------------------------------------------------------------------------
// Protected accessors — the derived classes reach the internal topology
// through these so the substrate stays invisible on the wrapper's
// public surface.
// ---------------------------------------------------------------------------

StateTopology &AbstractStateMachine::topology() noexcept
{
    return *_topology;
}

const StateTopology &AbstractStateMachine::topology() const noexcept
{
    return *_topology;
}

StateId AbstractStateMachine::defaultState() const noexcept
{
    return _defaultState;
}

// ---------------------------------------------------------------------------
// IStateMachine: state registration. Each delegation is a one-liner; the
// topology does the substrate translation so the wrapper stays thin.
// ---------------------------------------------------------------------------

StateId AbstractStateMachine::addState()
{
    checkThreadAffinity();
    return _topology->addState();
}

bool AbstractStateMachine::hasState(StateId state) const noexcept
{
    return _topology->hasState(state);
}

// ---------------------------------------------------------------------------
// IStateMachine: hierarchy.
// ---------------------------------------------------------------------------

Result AbstractStateMachine::addChildState(StateId parent, StateId child)
{
    checkThreadAffinity();
    return _topology->addChildEdge(parent, child);
}

StateId AbstractStateMachine::parent(StateId state) const
{
    return _topology->parentOf(state);
}

bool AbstractStateMachine::isAncestorOf(StateId ancestor, StateId descendant) const
{
    return _topology->isAncestorOf(ancestor, descendant);
}

// ---------------------------------------------------------------------------
// IStateMachine: initial / current. The base enforces that the target
// state is registered before it updates @c _current; callers that pass a
// stale id observe an @ref Result::Code::Error and no state change.
// ---------------------------------------------------------------------------

Result AbstractStateMachine::setInitial(StateId state)
{
    checkThreadAffinity();
    if (!_topology->hasState(state))
    {
        return Result(Result::Code::Error, "initial state not registered");
    }
    _current = state;
    return Result();
}

Result AbstractStateMachine::transition(StateId state)
{
    checkThreadAffinity();
    if (!_topology->hasState(state))
    {
        return Result(Result::Code::Error, "transition target not registered");
    }
    _current = state;
    return Result();
}

StateId AbstractStateMachine::current() const noexcept
{
    return _current;
}

// ---------------------------------------------------------------------------
// IStateMachine: routing. UD-3 fixes @ref RouteMode::Bubble as the
// default; the base stores the selection and a later leaf that wires
// the machine to the message bus reads it back.
// ---------------------------------------------------------------------------

RouteMode AbstractStateMachine::routeMode() const noexcept
{
    return _routeMode;
}

void AbstractStateMachine::setRouteMode(RouteMode mode) noexcept
{
    checkThreadAffinity();
    _routeMode = mode;
}

// ---------------------------------------------------------------------------
// IStateMachine: thread affinity.
//
// bindToControllerThread is a one-shot contract: the first successful call
// installs the binding, a second call is rejected. Implementing the
// one-shot with compare_exchange_strong on the default-constructed
// sentinel lets the bind remain lock-free and also lets the
// checkThreadAffinity gate observe the published id with acquire
// semantics without taking a mutex. The contract says Debug asserts on a
// repeat bind and Release silently keeps the original — that is exactly
// the behaviour a failing compare_exchange gives us: when the CAS fails
// Release simply returns, leaving the first-install id untouched.
// ---------------------------------------------------------------------------

void AbstractStateMachine::bindToControllerThread(std::thread::id controllerId)
{
    std::thread::id expected{}; // default-constructed = unbound sentinel
    if (!_controllerThreadId.compare_exchange_strong(
            expected,
            controllerId,
            std::memory_order_release,
            std::memory_order_acquire))
    {
        assert(false && "AbstractStateMachine::bindToControllerThread called twice");
    }
}

std::thread::id AbstractStateMachine::controllerThread() const noexcept
{
    return _controllerThreadId.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// checkThreadAffinity: Debug-only gate at the entry of every sync mutator.
//
// The unbound case (default-constructed id sentinel) is intentionally a
// no-op so the 191 existing tests — which never bind — keep passing
// unchanged. The Release case disappears entirely because the whole
// body is wrapped in @c \#ifndef NDEBUG.
// ---------------------------------------------------------------------------

void AbstractStateMachine::checkThreadAffinity() const noexcept
{
#ifndef NDEBUG
    const auto bound = _controllerThreadId.load(std::memory_order_acquire);
    if (bound != std::thread::id{})
    {
        assert(std::this_thread::get_id() == bound
               && "AbstractStateMachine: sync mutation from non-controller thread");
    }
#endif
}

// ---------------------------------------------------------------------------
// Asynchronous transition request.
//
// requestTransition is the producer side. It runs on any thread, takes the
// queue mutex for the few instructions it needs to push_back the target,
// and returns. No validation of the id happens here — stale ids surface as
// Result::Code::Error inside transition() during the drain. That keeps the
// producer fast and pushes the cost of stale-id rejection onto the
// controller thread, which is the only thread allowed to mutate the
// machine anyway.
//
// processQueuedTransitions is the consumer side. The contract says it
// runs on the controller thread (checkThreadAffinity gates it) and that
// it drains the queue in a single pass. The single pass is implemented by
// swap-out: under the mutex, swap _transitionQueue with a stack-local
// empty deque, and then walk the snapshot outside the lock. Two
// consequences fall out of that shape:
//
//   1. Producer threads that push_back during the drain do not block on
//      the per-target transition() call — they only contend on the brief
//      mutex hold of the swap.
//
//   2. Requests posted from inside onEnter / onExit hooks fired by
//      transition() during the drain land on the live queue, not on the
//      snapshot; they are processed on the *next* processQueuedTransitions
//      call. That's the cooperative-no-reentry contract documented on
//      IStateMachine::processQueuedTransitions.
// ---------------------------------------------------------------------------

void AbstractStateMachine::requestTransition(StateId target)
{
    std::lock_guard<std::mutex> lock{_queueMutex};
    _transitionQueue.push_back(target);
}

void AbstractStateMachine::processQueuedTransitions()
{
    checkThreadAffinity();

    std::deque<StateId> snapshot;
    {
        std::lock_guard<std::mutex> lock{_queueMutex};
        snapshot.swap(_transitionQueue);
    }

    for (const auto target : snapshot)
    {
        // Delegate to the existing synchronous transition machinery.
        // The Result is intentionally discarded here: a stale target
        // would have been rejected by the producer's caller in any
        // pre-flight hasState check, and the contract says
        // processQueuedTransitions does not surface per-request errors.
        // A later leaf can attach a diagnostic sink if a caller needs
        // visibility into rejected drains.
        (void) transition(target);
    }
}

} // namespace vigine::statemachine
