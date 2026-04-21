#include "vigine/messaging/abstractmessagetarget.h"

#include <cassert>
#include <utility>

#include "vigine/messaging/iconnectiontoken.h"

namespace vigine::messaging
{

void AbstractMessageTarget::acceptConnection(std::unique_ptr<IConnectionToken> token)
{
    std::lock_guard<std::mutex> lock{_connectionsMutex};
    _connections.push_back(std::move(token));
}

bool AbstractMessageTarget::canMove() const noexcept
{
    // Deliberately unsynchronised: the check is advisory only; callers
    // that race with a concurrent acceptConnection cannot rely on the
    // answer for correctness. The move constructor / move assignment
    // below asserts on the same predicate under a lock.
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
