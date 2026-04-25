#include "vigine/api/messaging/abstractmessagebus.h"

#include <vector>

#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/isubscriber.h"

namespace vigine::messaging
{

// FanOut: one-level breadth-first sweep over the subscription snapshot.
// Every matching subscriber receives the envelope regardless of the
// return value. Used by TopicPublish and ReactiveSignal traffic where
// multiple downstream consumers are expected to react to the same
// message independently.
void AbstractMessageBus::dispatchFanOut(
    const IMessage                     &message,
    const std::vector<SubscriptionSlot> &snapshot)
{
    const auto *target = message.target();
    for (const auto &slot : snapshot)
    {
        if (!matches(slot, message))
        {
            continue;
        }
        if (slot.filter.target != nullptr && slot.filter.target != target)
        {
            continue;
        }
        if (slot.subscriber == nullptr)
        {
            continue;
        }
        (void)deliver(slot, message);
    }
}

} // namespace vigine::messaging
