#include "vigine/messaging/abstractmessagebus.h"

#include <utility>
#include <vector>

#include "vigine/messaging/imessage.h"
#include "vigine/messaging/isubscriber.h"

namespace vigine::messaging
{

// FirstMatch: depth-first walk over the subscription snapshot with an
// early exit on the first subscriber that reports DispatchResult::Handled.
// Payload identity is checked through IMessage::payloadTypeId and the
// subscriber's own static_cast on the concrete payload type; the bus
// itself never reaches for runtime type information, which keeps the
// dispatch hot path as cheap as one virtual call per match per the
// plan_09 INV-1 mandate.
void AbstractMessageBus::dispatchFirstMatch(
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
        // When the slot filter pins a specific target, only deliver to
        // subscriptions whose filter pointer identifies the message
        // target. When the filter leaves the target open (nullptr),
        // the subscription accepts the message regardless of its
        // target (which includes null-addressed broadcast envelopes).
        if (slot.filter.target != nullptr && slot.filter.target != target)
        {
            continue;
        }
        if (slot.subscriber == nullptr)
        {
            continue;
        }
        const auto result = deliver(slot, message);
        if (result == DispatchResult::Handled || result == DispatchResult::Stop)
        {
            return;
        }
    }
}

} // namespace vigine::messaging
