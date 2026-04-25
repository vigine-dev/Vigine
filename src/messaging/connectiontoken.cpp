#include "vigine/messaging/connectiontoken.h"

#include <mutex>
#include <shared_mutex>
#include <utility>

#include "vigine/messaging/ibuscontrolblock.h"

namespace vigine::messaging
{

ConnectionToken::ConnectionToken(std::weak_ptr<IBusControlBlock> control,
                                 ConnectionId                   id,
                                 std::shared_ptr<SlotState>     slotState) noexcept
    : _control{std::move(control)}, _id{id}, _slotState{std::move(slotState)}
{
}

ConnectionToken::~ConnectionToken()
{
    // Delegate to cancel() so the explicit-cancel and RAII teardown
    // paths share one body. cancel() is idempotent via the
    // _cancelled atomic, so a user who already called cancel() pays
    // only the short-circuit check here; users who never called
    // cancel() run the full unregister-plus-barrier sequence exactly
    // once from inside the destructor. Mirrors the same delegation
    // pattern SubscriptionToken::~SubscriptionToken uses today.
    cancel();
}

void ConnectionToken::cancel()
{
    // Idempotency gate: the first caller flips false->true and runs
    // the full tear-down; any later caller sees true and short-
    // circuits before touching either the control block or the
    // slot's lifecycleMutex. acq_rel ordering pairs cleanly with the
    // acquire-load a future active()-style reader would need.
    bool expected = false;
    if (!_cancelled.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    // Step 1: defensive cancel barrier from the token side. Mirror of
    // `AbstractMessageBus::SubscriptionToken::cancel`. Acquired FIRST
    // so we block concurrent dispatch before the registry slot is
    // retired; on a dead bus (weak_ptr expired, registry reclaimed
    // en masse) step 2 becomes a no-op and this is the only barrier
    // that drains any dispatch snapshot still holding a copy of the
    // same SlotState. The `unique_lock` on the shared_mutex blocks
    // until every concurrent shared holder (every in-flight
    // `onMessage`) has released, which is the dtor-blocks contract
    // that the IConnectionToken header documents. The `cancelled`
    // store uses release ordering so the lock-free reader in
    // `active()` (which only does an acquire-load and never takes
    // this lock) sees the flip on its very next call.
    if (_slotState)
    {
        std::unique_lock<std::shared_mutex> lock{_slotState->lifecycleMutex};
        _slotState->cancelled.store(true, std::memory_order_release);
    }

    // Step 2: ask the control block to retire the registry slot. On
    // a live bus the block also runs the same exclusive-lock-flip-
    // cancelled sequence internally; the redundant second flip costs
    // only an uncontended lock acquisition and keeps the
    // unregisterTarget contract self-sufficient when callers reach
    // it through paths other than this token. On a dead bus the
    // weak_ptr.lock() returns null and the registry has already been
    // reclaimed — nothing to do in this branch.
    if (auto ctrl = _control.lock())
    {
        ctrl->unregisterTarget(_id);
    }
}

bool ConnectionToken::active() const noexcept
{
    // An inert token (registerTarget returned early on invalid input
    // or on shutdown) carries a default-constructed ConnectionId
    // whose generation is the zero sentinel. Such a token must
    // report active == false even when the bus is still alive,
    // otherwise a caller guarding an unregister with
    // `if (token.active())` would happily call unregister on an id
    // the bus never handed out.
    if (!_id.valid())
    {
        return false;
    }
    // An explicitly cancelled token is no longer active even if the
    // bus is still running. Mirrors the same check
    // `AbstractMessageBus::SubscriptionToken::active` runs on its
    // own `_cancelled` flag, so the two RAII tokens report a
    // consistent view of live/dead through `active()`.
    if (_cancelled.load(std::memory_order_acquire))
    {
        return false;
    }
    // The slot's shared SlotState may also be flipped from outside
    // this token — for example when the bus drives unregisterTarget
    // on its own (programmatic teardown) or when a sibling cancel
    // path on the same SlotState ran without going through this
    // token's own _cancelled atomic. The flag is `std::atomic<bool>`
    // and every writer pairs an exclusive `lifecycleMutex` (which
    // gives it the lifetime barrier for in-flight dispatches) with a
    // release store on the atomic (which gives it the visibility
    // pair for lock-free readers). This reader therefore needs only
    // an acquire load: the flag is monotonic — once it is `true` it
    // never returns to `false` — so observing it `true` is a
    // permanent retire signal, and observing it `false` is a
    // momentary live snapshot consistent with everything else
    // `active()` returns. Skipping the shared_lock here keeps the
    // hot path lock-free and removes the unnecessary contention
    // against concurrent dispatchers that already hold the same
    // shared_mutex shared.
    if (_slotState && _slotState->cancelled.load(std::memory_order_acquire))
    {
        return false;
    }
    auto ctrl = _control.lock();
    return static_cast<bool>(ctrl) && ctrl->isAlive();
}

} // namespace vigine::messaging
