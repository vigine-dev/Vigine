#include "vigine/actorhost/abstractactorhost.h"

namespace vigine::actorhost
{

AbstractActorHost::AbstractActorHost(vigine::messaging::IMessageBus    &bus,
                                     vigine::threading::IThreadManager &threadManager)
    : _bus(bus)
    , _threadManager(threadManager)
{
}

vigine::messaging::IMessageBus &AbstractActorHost::bus() noexcept
{
    return _bus;
}

vigine::threading::IThreadManager &AbstractActorHost::threadManager() noexcept
{
    return _threadManager;
}

} // namespace vigine::actorhost
