#include "vigine/impl/engine/enginetoken.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "vigine/api/context/icontext.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/api/service/iservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/signalemitter/isignalemitter.h"
#include "vigine/statemachine/abstractstatemachine.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/stateid.h"

namespace vigine::engine
{

// ---------------------------------------------------------------------------
// Construction / destruction.
//
// The constructor stamps the bound state on the @ref AbstractEngineToken base
// and immediately registers an invalidation listener on the state machine.
// The listener body lives on @ref onStateInvalidated which (under the bound
// state filter) flips @ref _alive and fires every registered expiration
// callback exactly once.
//
// Registration goes through @ref AbstractStateMachine::addInvalidationListener
// which is the FSM-level hook this leaf adds. The down-cast is safe at the
// engine wiring level: the engine builds its state machine through
// @ref createStateMachine which returns a @c DefaultStateMachine derived from
// @c AbstractStateMachine. A future leaf that swaps that factory for an
// alternative concrete state machine must keep the @c AbstractStateMachine
// base in the type chain so the listener registration path stays valid;
// the dynamic_cast guards against the case during development. When the
// down-cast fails we leave the listener unregistered — the token is
// effectively a permanently-alive handle in that scenario, which is the
// safest no-op fallback while keeping the engine bootable.
// ---------------------------------------------------------------------------

EngineToken::EngineToken(vigine::statemachine::StateId          boundState,
                         vigine::IContext                      &context,
                         vigine::statemachine::IStateMachine   &stateMachine,
                         vigine::signalemitter::ISignalEmitter *signalEmitter)
    : AbstractEngineToken(boundState),
      _context(context),
      _stateMachine(stateMachine),
      _signalEmitter(signalEmitter)
{
    // The IStateMachine wrapper interface does not surface the
    // invalidation-listener registry directly. The engine wiring
    // funnels every state machine through @ref createStateMachine which
    // hands back an @c AbstractStateMachine subclass; the down-cast
    // unlocks the listener registration without polluting the public
    // interface with the registry method. The cast is ergonomic, not a
    // correctness gate — the engine owns both ends of the wiring.
    if (auto *abstractMachine =
            dynamic_cast<vigine::statemachine::AbstractStateMachine *>(&_stateMachine))
    {
        _invalidationSub = abstractMachine->addInvalidationListener(
            [this](vigine::statemachine::StateId leavingState)
            {
                onStateInvalidated(leavingState);
            });
    }
}

EngineToken::~EngineToken()
{
    // Drop the FSM-side subscription explicitly so the listener no
    // longer fires after the token's vtable has been destroyed; the
    // @c unique_ptr destructor would do this anyway, but ordering it
    // first keeps the teardown sequence obvious. Any in-flight
    // expiration callbacks have already returned because the listener
    // path holds no token mutex across user code (snapshot pattern).
    _invalidationSub.reset();
}

// ---------------------------------------------------------------------------
// Gated domain accessors.
//
// The first thing every gated accessor does is observe @ref isAlive. A false
// result short-circuits to @ref Result::Code::Expired without touching the
// context — that is the whole point of the alive flag and of the listener
// firing BEFORE the FSM flips @c _current.
//
// Live accessors delegate to the context. The context does not yet expose
// every surface the IEngineToken contract names (the entity manager and the
// component manager remain stubs as of #220 and the wider #197 follow-ups);
// those accessors return @ref Result::Code::Unavailable so callers can branch
// on a typed reason without observing a null reference. The signature still
// commits to the eventual surface so call sites do not rewrite when the
// follow-up wiring lands.
// ---------------------------------------------------------------------------

Result<vigine::service::IService &>
EngineToken::service(vigine::service::ServiceId id)
{
    using R = Result<vigine::service::IService &>;
    if (!isAlive())
    {
        return R::failure(R::Code::Expired);
    }
    if (!id.valid())
    {
        return R::failure(R::Code::NotFound);
    }

    auto svc = _context.service(id);
    if (!svc)
    {
        return R::failure(R::Code::NotFound);
    }
    return R::ok(*svc);
}

Result<vigine::ecs::ISystem &>
EngineToken::system(vigine::SystemId /*id*/)
{
    using R = Result<vigine::ecs::ISystem &>;
    if (!isAlive())
    {
        return R::failure(R::Code::Expired);
    }
    // The ecs::ISystem wrapper surface lands in a follow-up under the
    // #197 umbrella. Until the IECS surface gains a string-keyed
    // system locator, this accessor reports a typed reason for the
    // miss instead of dereferencing a stub.
    return R::failure(R::Code::Unavailable);
}

Result<vigine::IEntityManager &> EngineToken::entityManager()
{
    using R = Result<vigine::IEntityManager &>;
    if (!isAlive())
    {
        return R::failure(R::Code::Expired);
    }
    // The entity manager is a forward-declared stub today
    // (`include/vigine/api/ecs/ientitymanager.h`). The IContext
    // aggregator does not yet hand one out; @ref ecs() exposes the
    // wrapper instead. A follow-up leaf wires the entity manager
    // through and flips this branch to @ref Result::Code::Ok.
    return R::failure(R::Code::Unavailable);
}

Result<vigine::IComponentManager &> EngineToken::components()
{
    using R = Result<vigine::IComponentManager &>;
    if (!isAlive())
    {
        return R::failure(R::Code::Expired);
    }
    // Same status as @ref entityManager — the component manager
    // wrapper is an interface stub waiting on its concrete leaf.
    return R::failure(R::Code::Unavailable);
}

Result<vigine::ecs::IECS &> EngineToken::ecs()
{
    using R = Result<vigine::ecs::IECS &>;
    if (!isAlive())
    {
        return R::failure(R::Code::Expired);
    }
    // The context never returns a partial ECS handle — its destructor
    // tears the wrapper down only when the context itself dies, so a
    // live context yields a live wrapper. No null guard needed.
    return R::ok(_context.ecs());
}

// ---------------------------------------------------------------------------
// Ungated infrastructure accessors.
//
// Per the R-StateScope hybrid policy, the resources behind these accessors
// outlive every state transition (they are owned by the context for the
// engine's whole lifetime). The token forwards them straight through, alive
// flag or not — a task that has already observed @ref isAlive going false
// can still drain its in-flight scheduling, finish its bus posting, and so on
// before it drops the token.
// ---------------------------------------------------------------------------

vigine::core::threading::IThreadManager &EngineToken::threadManager() noexcept
{
    return _context.threadManager();
}

vigine::messaging::IMessageBus &EngineToken::systemBus() noexcept
{
    return _context.systemBus();
}

vigine::signalemitter::ISignalEmitter &EngineToken::signalEmitter() noexcept
{
    // The IContext aggregator does not expose a signal-emitter accessor
    // today — the ISignalEmitter wrapper is plumbed through follow-up
    // leaves under the #197 umbrella. Until then, the token only honours
    // calls when the engine wiring passes a non-null pointer to the
    // constructor; otherwise this accessor terminates so a misuse is
    // visible immediately rather than silently dereferencing a null.
    if (_signalEmitter == nullptr)
    {
        std::abort();
    }
    return *_signalEmitter;
}

vigine::statemachine::IStateMachine &EngineToken::stateMachine() noexcept
{
    return _stateMachine;
}

// ---------------------------------------------------------------------------
// Expiration notification.
//
// Subscribers register a callback through @ref subscribeExpiration. The
// callback fires exactly once, on whichever thread runs the FSM transition
// that vacates the bound state — typically the controller thread.
//
// Defensive same-thread fire: when a caller registers AFTER the token has
// already invalidated, the contract says the callback fires "as soon as
// the engine has finished its housekeeping". The simplest honoured shape
// is "fire inline, on the registering thread" — the alternative
// (post-back through the engine's thread manager) carries strictly more
// timing surface for no behavioural gain, since the registering caller
// has already observed an expired token.
// ---------------------------------------------------------------------------

std::unique_ptr<vigine::messaging::ISubscriptionToken>
EngineToken::subscribeExpiration(std::function<void()> callback)
{
    if (!callback)
    {
        // Inert token: id == 0 keeps @ref cancelExpirationCallback a
        // no-op for it.
        return std::make_unique<ExpirationToken>(this, 0u);
    }

    // If the token has already expired, fire the callback immediately
    // and hand back an inert subscription. Reading the latch with
    // acquire semantics happens-before the alive flag drop, so this
    // branch only ever runs after @ref onStateInvalidated has already
    // walked its snapshot and returned.
    if (_expirationFired.load(std::memory_order_acquire))
    {
        callback();
        return std::make_unique<ExpirationToken>(this, 0u);
    }

    std::scoped_lock lock{_expirationMutex};

    // Re-check under the lock to close the small window between the
    // unlocked latch read above and the registration: a transition may
    // have fired the callbacks while we were entering the critical
    // section. This guarantees the "exactly once" contract regardless
    // of how the registering thread races the firing thread.
    if (_expirationFired.load(std::memory_order_acquire))
    {
        // Drop the lock before invoking the callback so a callback
        // that re-enters the token (e.g. queries @ref isAlive) does
        // not deadlock on the registry mutex.
        std::unique_lock unlock{_expirationMutex, std::adopt_lock};
        unlock.unlock();
        callback();
        return std::make_unique<ExpirationToken>(this, 0u);
    }

    const std::uint32_t id =
        _nextExpirationId.fetch_add(1, std::memory_order_acq_rel);
    _expirationCallbacks.push_back(ExpirationSlot{id, std::move(callback)});
    return std::make_unique<ExpirationToken>(this, id);
}

// ---------------------------------------------------------------------------
// FSM listener body.
//
// The state machine fires this callback on every non-noop, valid transition.
// The token only acts when the leaving state matches its bound state — every
// other notification belongs to a different state-bound token.
// ---------------------------------------------------------------------------

void EngineToken::onStateInvalidated(vigine::statemachine::StateId leavingState)
{
    if (leavingState != boundState())
    {
        return;
    }

    // Order matters:
    //   1. Fire the registered expiration callbacks first. They run on
    //      the controller thread synchronously inside the FSM listener
    //      path. The alive flag is still true at this point; a
    //      callback that re-reads @ref isAlive would observe the live
    //      state. That is the documented order — observers see
    //      "callback runs, THEN alive flips false" — so a callback can
    //      still issue last-mile cleanup that depends on a live token.
    //   2. Mark the token expired. After this @ref isAlive flips false
    //      and every subsequent gated accessor short-circuits to
    //      @ref Result::Code::Expired.
    fireExpirationCallbacks();
    markExpired();
}

void EngineToken::fireExpirationCallbacks()
{
    // Snapshot the callback list under the mutex, set the latch, then
    // run the callbacks outside the lock. The latch flip under the
    // mutex is what makes the "exactly once" contract racy-safe: a
    // second invalidation post-back (no matter how it arrived) sees a
    // raised latch and returns without firing the callbacks again.
    std::vector<std::function<void()>> snapshot;
    {
        std::scoped_lock lock{_expirationMutex};
        if (_expirationFired.load(std::memory_order_acquire))
        {
            return;
        }
        snapshot.reserve(_expirationCallbacks.size());
        for (const auto &slot : _expirationCallbacks)
        {
            if (slot.callback)
            {
                snapshot.push_back(slot.callback);
            }
        }
        _expirationFired.store(true, std::memory_order_release);
        // Clear the registry so a token cancel call after the firing
        // path is a cheap no-op — the slot list is empty so the
        // matching id miss returns immediately. Holding the cleared
        // vector also frees the captured callback's references
        // promptly, which can matter for callbacks that own large
        // closures.
        _expirationCallbacks.clear();
    }

    for (auto &cb : snapshot)
    {
        cb();
    }
}

void EngineToken::cancelExpirationCallback(std::uint32_t id) noexcept
{
    if (id == 0u)
    {
        return;
    }
    std::scoped_lock lock{_expirationMutex};
    for (auto &slot : _expirationCallbacks)
    {
        if (slot.id == id)
        {
            slot.callback = nullptr;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// ExpirationToken — RAII handle over @ref EngineToken's expiration registry.
// ---------------------------------------------------------------------------

EngineToken::ExpirationToken::ExpirationToken(EngineToken *owner,
                                              std::uint32_t id) noexcept
    : _owner(owner), _id(id)
{
}

EngineToken::ExpirationToken::~ExpirationToken()
{
    cancel();
}

void EngineToken::ExpirationToken::cancel() noexcept
{
    const std::uint32_t id = _id.exchange(0u, std::memory_order_acq_rel);
    if (id != 0u && _owner != nullptr)
    {
        _owner->cancelExpirationCallback(id);
    }
}

bool EngineToken::ExpirationToken::active() const noexcept
{
    return _id.load(std::memory_order_acquire) != 0u;
}

} // namespace vigine::engine
