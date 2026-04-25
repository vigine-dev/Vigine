#include "vigine/api/messaging/abstractmessagetarget.h"

#include <cassert>
#include <utility>

#include "vigine/api/messaging/iconnectiontoken.h"

namespace vigine::messaging
{

void AbstractMessageTarget::acceptConnection(std::unique_ptr<IConnectionToken> token)
{
    std::lock_guard<std::mutex> lock{_connectionsMutex};
    _connections.push_back(std::move(token));
}

bool AbstractMessageTarget::canMove() const noexcept
{
    // Must hold `_connectionsMutex` while reading `_connections` — even
    // for an advisory answer. `std::vector` is not TSAN-safe and an
    // unsynchronised `empty()` racing with a concurrent
    // `acceptConnection()` (which does `push_back`) is a C++ data
    // race (UB), not merely a stale read. Contention is negligible in
    // practice: `canMove()` fires at move boundaries, which are rare.
    // The move constructor / move assignment below take the same lock
    // before asserting on the same predicate, so the pre-move advisory
    // probe matches what the move itself would see.
    std::lock_guard<std::mutex> lock{_connectionsMutex};
    return _connections.empty();
}

AbstractMessageTarget::AbstractMessageTarget(AbstractMessageTarget &&other) noexcept
{
    std::lock_guard<std::mutex> lock{other._connectionsMutex};
    assert(other._connections.empty()
           && "AbstractMessageTarget move: source is still registered");
    _connections = std::move(other._connections);
}

AbstractMessageTarget &
AbstractMessageTarget::operator=(AbstractMessageTarget &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    std::scoped_lock<std::mutex, std::mutex> lock{_connectionsMutex,
                                                  other._connectionsMutex};
    assert(_connections.empty()
           && "AbstractMessageTarget move-assign: destination is still registered");
    assert(other._connections.empty()
           && "AbstractMessageTarget move-assign: source is still registered");
    _connections = std::move(other._connections);
    return *this;
}

} // namespace vigine::messaging
