#include "vigine/messaging/abstractmessagebus.h"

#include <vector>

#include "vigine/messaging/imessage.h"
#include "vigine/messaging/isubscriber.h"

namespace vigine::messaging
{

// Broadcast: every subscriber that matches the kind / payload-type-id /
// expectedRoute filter receives the message, regardless of the filter's
// target pointer. Used by Control traffic (lifecycle, shutdown) and by
// facades that need an unconditional fan-out (Event with Broadcast
// selection).
void AbstractMessageBus::dispatchBroadcast(
    const IMessage                     &message,
    const std::vector<SubscriptionSlot> &snapshot)
{
    for (const auto &slot : snapshot)
    {
        if (!matches(slot, message))
        {
            continue;
        }
        if (slot.subscriber == nullptr)
        {
            continue;
        }
        (void)deliver(*slot.subscriber, message);
    }
}

} // namespace vigine::messaging
