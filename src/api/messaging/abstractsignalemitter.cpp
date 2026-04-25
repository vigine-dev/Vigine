#include "vigine/api/messaging/abstractsignalemitter.h"

#include <utility>

#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"

namespace vigine::messaging
{

AbstractSignalEmitter::AbstractSignalEmitter(
    vigine::messaging::BusConfig      config,
    vigine::core::threading::IThreadManager &threadManager)
    : vigine::messaging::AbstractMessageBus{std::move(config), threadManager}
{
}

std::unique_ptr<vigine::messaging::ISubscriptionToken>
AbstractSignalEmitter::subscribeSignal(
    vigine::messaging::MessageFilter filter,
    vigine::messaging::ISubscriber  *subscriber)
{
    // Force the kind to Signal so this entry point is always scoped to
    // signal traffic regardless of what the caller placed in the filter.
    filter.kind = vigine::messaging::MessageKind::Signal;
    return vigine::messaging::AbstractMessageBus::subscribe(filter, subscriber);
}

} // namespace vigine::messaging
