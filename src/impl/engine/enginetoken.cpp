#include "vigine/impl/engine/enginetoken.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "vigine/api/context/icontext.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/api/ecs/ientitymanager.h"
#include "vigine/api/engine/iengine.h"
#include "vigine/api/service/iservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/api/messaging/abstractmessagetarget.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/result.h"
#include "vigine/api/messaging/isignalemitter.h"
#include "vigine/api/messaging/payload/isignalpayload.h"
#include "vigine/api/statemachine/abstractstatemachine.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"

namespace vigine::engine
{

namespace
{

// ---------------------------------------------------------------------------
// NullSignalEmitter -- file-private no-op stub used when the engine wiring
// does not yet pass a real ISignalEmitter to the token (the ISignalEmitter
// follow-up under #197 lands separately, see #283).
//
// Returning a stub object instead of std::abort() lets the IEngineToken
// contract's "infrastructure accessor cannot fail" promise hold even when
// the wiring is incomplete: callers obtain a live reference whose every
// emit/emitTo/subscribeSignal is a quiet no-op. The stub has no state, no
// lifetime obligations beyond the EngineToken that owns it, and never
// reaches outside its translation unit.
// ---------------------------------------------------------------------------
class NullSignalEmitter final : public vigine::messaging::ISignalEmitter
{
  public:
    NullSignalEmitter() = default;

    [[nodiscard]] vigine::Result emit(
        std::unique_ptr<vigine::messaging::ISignalPayload> /*payload*/) override
    {
        // Drop the payload silently; the stub stands in for an unwired
        // emitter and intentionally does nothing. Callers that need to
        // observe delivery should not be calling through this stub.
        return vigine::Result();
    }

    [[nodiscard]] vigine::Result emitTo(
        const vigine::messaging::AbstractMessageTarget * /*target*/,
        std::unique_ptr<vigine::messaging::ISignalPayload> /*payload*/) override
    {
        return vigine::Result();
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeSignal(vigine::messaging::MessageFilter /*filter*/,
                        vigine::messaging::ISubscriber * /*subscriber*/) override
    {
        // The contract says a null subscriber yields a null token; the
        // stub generalises that: every subscription is inert because
        // nothing will ever be emitted on this stub.
        return nullptr;
    }
};

} // namespace

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
// @ref createStateMachine which returns a @c StateMachine derived from
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
                         vigine::messaging::ISignalEmitter *signalEmitter)
    : AbstractEngineToken(boundState),
      _context(context),
      _stateMachine(stateMachine),
      _ownedNullSignalEmitter(signalEmitter == nullptr
                                  ? std::make_unique<NullSignalEmitter>()
                                  : nullptr),
      _signalEmitter(signalEmitter != nullptr ? signalEmitter
                                              : _ownedNullSignalEmitter.get())
{
    // The IEngineToken contract documents @ref signalEmitter as an
    // ungated infrastructure accessor that always returns a live
    // reference. When the engine wiring does not yet pass a real
    // ISignalEmitter (the wrapper follow-up under #283 lands separately
    // from this leaf) the constructor falls back to a file-private
    // NullSignalEmitter stub so the accessor honours its "cannot fail"
    // contract regardless of the wiring state. Once the real wrapper
    // is wired through IContext, callers pass a non-null pointer and
    // the stub stays default-empty.
    assert(_signalEmitter != nullptr
           && "EngineToken: _signalEmitter must be non-null after construction");
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

    // Debug-only lifetime invariant guard: no @ref ExpirationToken
    // is allowed to outlive its owning @ref EngineToken (see the
    // ExpirationToken docstring in the header). The token's
    // @ref ExpirationToken::cancel path dereferences the raw
    // @c _owner back-pointer, so a token that survives its owner
    // would dereference freed storage. Tracking the live-handle
    // count lets us surface the misuse immediately under Debug;
    // Release skips the check to keep teardown cost zero.
    assert(_liveExpirationTokens.load(std::memory_order_acquire) == 0u
           && "EngineToken: ExpirationToken outlived its owning EngineToken");
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
    // The IContext aggregator owns a default @ref IEntityManager
    // through @ref AbstractContext::_entityManager (and any caller-
    // side override installed via @ref IContext::setEntityManager).
    // The slot is non-null for the context's lifetime, so the
    // reference @ref IContext::entityManager hands back is always
    // live; the alive-state gate above is the only failure path the
    // accessor needs to honour.
    return R::ok(_context.entityManager());
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

vigine::messaging::ISignalEmitter &EngineToken::signalEmitter() noexcept
{
    // _signalEmitter is non-null by the constructor's post-condition:
    // when the engine passes a real wrapper we point at it; otherwise
    // we fall back to the NullSignalEmitter stub owned by the token
    // (see _ownedNullSignalEmitter). Either way the accessor honours
    // the "ungated infrastructure accessor cannot fail" promise of
    // the IEngineToken contract — callers always observe a live
    // reference, never a crashed program.
    return *_signalEmitter;
}

vigine::statemachine::IStateMachine &EngineToken::stateMachine() noexcept
{
    return _stateMachine;
}

vigine::engine::IEngine &EngineToken::engine() noexcept
{
    // The engine outlives the context (the engine OWNS the context)
    // and therefore the token, so the back-pointer wired in by the
    // engine's constructor body is valid for the token's entire
    // lifetime. Forwarding through the context keeps the
    // single-source-of-truth invariant: every accessor that points at
    // an engine-lifetime resource resolves it through @ref IContext.
    return _context.engine();
}

// ---------------------------------------------------------------------------
// Expiration notification.
//
// Subscribers register a callback through @ref subscribeExpiration. The
// callback fires exactly once on whichever thread runs the FSM transition
// that vacates the bound state — the controller thread, by the
// IStateMachine thread-affinity contract.
//
// Per the IEngineToken contract, @ref subscribeExpiration returns a null
// subscription token when the supplied callback is empty OR when the
// token has already expired by the time the registration arrives. Both
// branches honour the contract literally without invoking the callback;
// the registering caller (which has already observed an expired token,
// or which never had a callback to begin with) is responsible for any
// fall-back logic that would otherwise have run on the firing path.
//
// Locking pattern (Pattern B): the registry mutex is held only for the
// slot mutation. We never hold the mutex across the user-supplied
// callback. The firing path uses the same shape -- snapshot the active
// callbacks under the mutex, release the mutex, then walk the snapshot.
// This keeps callback bodies free to call back into the token (e.g.
// @ref isAlive, @ref signalEmitter) without deadlocking on themselves.
// ---------------------------------------------------------------------------

std::unique_ptr<vigine::messaging::ISubscriptionToken>
EngineToken::subscribeExpiration(std::function<void()> callback)
{
    // The IEngineToken contract says: "Returns a null subscription
    // token when @p callback is empty or when the token is already
    // expired at registration time." Both early-return paths honour
    // that contract literally.
    if (!callback)
    {
        return nullptr;
    }

    // Cheap unlocked latch read. If the latch is already raised the
    // expiration callbacks have either already fired or are about to
    // fire on the FSM transition thread; either way no new
    // registration can usefully observe an expiration that already
    // happened, so the contract returns a null token without
    // attempting to register.
    if (_expirationFired.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    // Take the registry mutex for the slot mutation only. We never
    // run the user-supplied @p callback while holding this mutex --
    // the firing path below uses the snapshot-and-fire pattern that
    // copies the active callbacks aside under the lock and then
    // releases the lock before invoking any of them.
    std::uint32_t id = 0u;
    {
        std::scoped_lock lock{_expirationMutex};

        // Re-check under the lock to close the small window between
        // the unlocked latch read above and the registration. If the
        // firing path got here first the latch is now raised and we
        // honour the contract by returning a null token without
        // registering anything.
        if (_expirationFired.load(std::memory_order_acquire))
        {
            return nullptr;
        }

        // Reuse-on-add: walk the registry looking for a cancelled
        // slot (empty callback) we can repopulate before appending a
        // new entry. The slot list is append-only at the firing site
        // (ids never shift), so reuse-on-add keeps the list bounded
        // by the live-subscription count even when a long-running
        // task churns through many short-lived subscriptions.
        for (auto &slot : _expirationCallbacks)
        {
            if (!slot.callback)
            {
                slot.id =
                    _nextExpirationId.fetch_add(1, std::memory_order_acq_rel);
                slot.callback = std::move(callback);
                id            = slot.id;
                break;
            }
        }
        if (id == 0u)
        {
            id = _nextExpirationId.fetch_add(1, std::memory_order_acq_rel);
            _expirationCallbacks.push_back(ExpirationSlot{id, std::move(callback)});
        }
    }
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
    //   1. Fire the registered expiration callbacks first. They run
    //      synchronously on whichever thread executed the FSM
    //      transition (the controller thread, by the IStateMachine
    //      thread-affinity contract). The alive flag is still true
    //      at this point; a callback that re-reads @ref isAlive
    //      would observe the live state. That is the documented
    //      order -- observers see "callback runs, THEN alive flips
    //      false" -- so a callback can still issue last-mile cleanup
    //      that depends on a live token.
    //   2. Mark the token expired. After this @ref isAlive flips
    //      false and every subsequent gated accessor short-circuits
    //      to @ref Result::Code::Expired.
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
    // Bump the owner's live-token counter so the owner's destructor
    // can assert the lifetime invariant (see ExpirationToken
    // docstring in the header). Inert tokens (id == 0) and tokens
    // with a null owner do not participate -- they have nothing to
    // cancel, so they cannot violate the invariant. The matching
    // decrement happens exactly once, on whichever of @ref cancel
    // or the destructor first observes the active registration.
    if (_owner != nullptr && id != 0u)
    {
        _owner->_liveExpirationTokens.fetch_add(1, std::memory_order_acq_rel);
    }
}

EngineToken::ExpirationToken::~ExpirationToken()
{
    cancel();
}

void EngineToken::ExpirationToken::cancel() noexcept
{
    // Exchange the id to zero so a concurrent cancel / destructor
    // pair calls @ref cancelExpirationCallback at most once with the
    // same id. The thread that observes a non-zero pre-exchange value
    // is the one that owns the bookkeeping (the registry slot drop
    // AND the matching decrement on @ref _liveExpirationTokens).
    const std::uint32_t id = _id.exchange(0u, std::memory_order_acq_rel);
    if (id != 0u && _owner != nullptr)
    {
        _owner->cancelExpirationCallback(id);
        _owner->_liveExpirationTokens.fetch_sub(1, std::memory_order_acq_rel);
    }
}

bool EngineToken::ExpirationToken::active() const noexcept
{
    return _id.load(std::memory_order_acquire) != 0u;
}

} // namespace vigine::engine
