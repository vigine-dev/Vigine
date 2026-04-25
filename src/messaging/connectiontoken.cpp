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
    // Step 1: ask the control block to retire the registry slot. On a
    // live bus the block also drains every dispatch in flight on this
    // slot by acquiring `_slotState->lifecycleMutex` exclusively
    // before flipping `cancelled`. On a dead bus the weak_ptr.lock()
    // returns null and the registry has already been reclaimed en
    // masse — the cancel barrier below still has work to do for any
    // dispatch snapshot still holding a copy of the same SlotState.
    if (auto ctrl = _control.lock())
    {
        ctrl->unregisterTarget(_id);
    }

    // Step 2: defensive cancel barrier from the token side. Mirror of
    // `SubscriptionToken::cancel`. Even when step 1 ran the same
    // logic inside `unregisterTarget`, repeating it here costs only an
    // already-uncontended lock acquisition and covers the bus-died-
    // first path, where step 1 was a no-op but a racing dispatch may
    // still be sitting on a snapshot copy of `_slotState`. The
    // `unique_lock` on the shared_mutex blocks until every concurrent
    // shared holder (every in-flight `onMessage`) has released, which
    // is the dtor-blocks contract that the IConnectionToken header
    // documents.
    if (_slotState)
    {
        std::unique_lock<std::shared_mutex> lock{_slotState->lifecycleMutex};
        _slotState->cancelled = true;
    }
}

bool ConnectionToken::active() const noexcept
{
    // An inert token (subscribe returned early on invalid input or on
    // shutdown) carries a default-constructed ConnectionId whose
    // generation is the zero sentinel. Such a token must report
    // active == false even when the bus is still alive, otherwise a
    // caller guarding an unsubscribe with `if (token.active())` would
    // happily call unsubscribe on an id the bus never handed out.
    if (!_id.valid())
    {
        return false;
    }
    auto ctrl = _control.lock();
    return static_cast<bool>(ctrl) && ctrl->isAlive();
}

} // namespace vigine::messaging
