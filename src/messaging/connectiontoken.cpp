#include "vigine/messaging/connectiontoken.h"

#include <utility>

#include "vigine/messaging/ibuscontrolblock.h"

namespace vigine::messaging
{

ConnectionToken::ConnectionToken(std::weak_ptr<IBusControlBlock> control,
                                 ConnectionId                   id) noexcept
    : _control{std::move(control)}, _id{id}
{
}

ConnectionToken::~ConnectionToken()
{
    // First, prevent new dispatches on this slot. If the bus is still
    // alive we ask it to drop our registry entry; if it died first, the
    // weak lock fails and we fall through to the drain wait.
    if (auto ctrl = _control.lock())
    {
        ctrl->unregisterTarget(_id);
    }

    // Strong-unsubscribe barrier: wait until every dispatch that
    // observed the slot (before unregisterTarget took effect) has
    // finished calling AbstractMessageTarget::onMessage. The bus is
    // responsible for incrementing _inFlight before the call and
    // decrementing it after the call; endDispatch signals the cv on
    // the 1 -> 0 transition.
    std::unique_lock<std::mutex> lock{_waitMutex};
    _waitCv.wait(lock, [this] {
        return _inFlight.load(std::memory_order_acquire) == 0;
    });
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

void ConnectionToken::beginDispatch() noexcept
{
    _inFlight.fetch_add(1, std::memory_order_acq_rel);
}

void ConnectionToken::endDispatch() noexcept
{
    if (_inFlight.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        std::lock_guard<std::mutex> lock{_waitMutex};
        _waitCv.notify_all();
    }
}

} // namespace vigine::messaging
