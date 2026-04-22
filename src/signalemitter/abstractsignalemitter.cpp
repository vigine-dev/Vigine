#include "vigine/signalemitter/abstractsignalemitter.h"

#include <utility>

#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"

namespace vigine::signalemitter
{

AbstractSignalEmitter::AbstractSignalEmitter(
    vigine::messaging::BusConfig      config,
    vigine::threading::IThreadManager &threadManager)
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

} // namespace vigine::signalemitter
