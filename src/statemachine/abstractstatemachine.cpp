#include "vigine/statemachine/abstractstatemachine.h"

#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

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

    // Capture the state we are leaving before we flip @c _current.
    // The invalidation hook fires only on a genuine state change; a
    // no-op transition (target equals the current id) propagates no
    // notification because no engine token bound to the current state
    // has actually been invalidated.
    const StateId oldState = _current.load(std::memory_order_acquire);
    if (oldState == state)
    {
        return Result();
    }

    // Fire listeners BEFORE the @c _current flip so observers that
    // call back into @c current() (or otherwise inspect machine
    // state) consistently see the OLD active state. Listeners run on
    // the controller thread, synchronously, outside the registry
    // mutex (snapshot pattern in @ref fireInvalidationListeners) so a
    // listener that issues a follow-up @ref requestTransition or
    // registers another listener cannot deadlock.
    fireInvalidationListeners(oldState);

    _current.store(state, std::memory_order_release);
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
// and returns. No validation of the id happens here — the drain delegates
// each entry to the synchronous transition() and intentionally discards
// the per-target Result, so a stale id is silently dropped on the
// controller thread instead of being reported back to the producer.
// That keeps the producer fast; callers that need pre-flight validation
// run hasState() before requestTransition().
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
        // The Result is intentionally discarded: the contract says
        // processQueuedTransitions returns void and does not surface
        // per-request failures (e.g. a stale @ref StateId rejected by
        // @ref transition). Failures are deliberately swallowed by
        // this leaf — callers that need pre-flight validation use
        // @ref hasState before @ref requestTransition. A separate
        // future leaf may attach a diagnostic sink (callback or
        // aggregate Result) if a caller needs visibility into the
        // rejected drains; that is out of scope here.
        //
        // Invalidation listeners fire from inside @ref transition on
        // every non-noop, valid drain entry, so observers reach the
        // same hook from sync and async transitions alike.
        (void) transition(target);
    }
}

// ---------------------------------------------------------------------------
// Invalidation-listener registry.
//
// Listeners are stored in a small @c std::vector under
// @c _invalidationListenersMutex. Each successful register call appends a
// slot keyed by a monotonic id; the matching @ref InvalidationSubscriptionToken
// holds the same id and uses it on its destructor / @c cancel path to find
// the slot back. Cancellation does not erase the slot — it clears the
// callback so the firing path (which iterates a snapshot taken under the
// mutex) skips the entry. The vector therefore stays append-only at the
// firing site and listener identities never shift, which keeps the
// monotonic-id <-> slot mapping stable for the token's lifetime.
//
// Firing:
//   * The mutex is held only long enough to copy the active callbacks
//     into a stack-local snapshot. Listeners themselves run outside the
//     lock so a listener that re-enters the FSM (e.g. requests another
//     transition or registers another listener) does not deadlock on
//     itself.
//   * Listeners run on the controller thread synchronously. The FSM does
//     not catch listener-side exceptions: a throwing listener propagates
//     through @ref transition and surfaces to the caller, intentionally,
//     so engine-token subscription bugs are visible.
// ---------------------------------------------------------------------------

std::unique_ptr<vigine::messaging::ISubscriptionToken>
AbstractStateMachine::addInvalidationListener(std::function<void(StateId)> listener)
{
    if (!listener)
    {
        // Match the @ref IMessageBus::subscribe convention: an empty
        // callback yields an inert token whose @c active is false from
        // the start. Returning a token with id == 0 makes
        // @ref cancelInvalidationListener a no-op for it.
        return std::make_unique<InvalidationSubscriptionToken>(this, 0u);
    }

    std::scoped_lock lock{_invalidationListenersMutex};
    const std::uint32_t id =
        _nextInvalidationListenerId.fetch_add(1, std::memory_order_acq_rel);
    _invalidationListeners.push_back(InvalidationListenerSlot{id, std::move(listener)});
    return std::make_unique<InvalidationSubscriptionToken>(this, id);
}

void AbstractStateMachine::cancelInvalidationListener(std::uint32_t id) noexcept
{
    if (id == 0u)
    {
        return;
    }
    std::scoped_lock lock{_invalidationListenersMutex};
    for (auto &slot : _invalidationListeners)
    {
        if (slot.id == id)
        {
            // Clear the callback rather than erase the slot. The firing
            // path takes a snapshot of the vector and walks it under no
            // lock; an erase would shift indices and racing snapshots
            // could observe a stale callback at the wrong index.
            // Clearing is enough — the snapshot path skips empty slots.
            slot.callback = nullptr;
            return;
        }
    }
}

void AbstractStateMachine::fireInvalidationListeners(StateId oldState)
{
    // Take a snapshot of the active callbacks under the mutex, then
    // release the mutex before invoking any listener. This ordering
    // matches the standard @c IMessageBus dispatch shape and keeps
    // listener bodies free to call back into the FSM without
    // deadlocking on the registry mutex.
    std::vector<std::function<void(StateId)>> snapshot;
    {
        std::scoped_lock lock{_invalidationListenersMutex};
        snapshot.reserve(_invalidationListeners.size());
        for (const auto &slot : _invalidationListeners)
        {
            if (slot.callback)
            {
                snapshot.push_back(slot.callback);
            }
        }
    }

    for (auto &cb : snapshot)
    {
        cb(oldState);
    }
}

// ---------------------------------------------------------------------------
// InvalidationSubscriptionToken — RAII handle returned by
// @ref AbstractStateMachine::addInvalidationListener.
// ---------------------------------------------------------------------------

AbstractStateMachine::InvalidationSubscriptionToken::InvalidationSubscriptionToken(
    AbstractStateMachine *owner,
    std::uint32_t         id) noexcept
    : _owner(owner), _id(id)
{
}

AbstractStateMachine::InvalidationSubscriptionToken::~InvalidationSubscriptionToken()
{
    cancel();
}

void AbstractStateMachine::InvalidationSubscriptionToken::cancel() noexcept
{
    // Atomically fetch-and-clear the id so a concurrent @c cancel /
    // destructor pair does not call @c cancelInvalidationListener twice
    // with the same id. The @c noexcept @ref cancelInvalidationListener
    // is itself idempotent on a stale id, but doing the work here keeps
    // the cost bounded to one @c std::scoped_lock acquisition per token.
    const std::uint32_t id = _id.exchange(0u, std::memory_order_acq_rel);
    if (id != 0u && _owner != nullptr)
    {
        _owner->cancelInvalidationListener(id);
    }
}

bool AbstractStateMachine::InvalidationSubscriptionToken::active() const noexcept
{
    return _id.load(std::memory_order_acquire) != 0u;
}

} // namespace vigine::statemachine
