#include "vigine/api/messaging/abstractmessagebus.h"

#include <vector>

#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/isubscriber.h"

namespace vigine::messaging
{

// Chain: linear traversal over the subscription snapshot. Subscribers
// return DispatchResult::Pass to let the next stage run,
// DispatchResult::Handled to report that the chain finished normally,
// and DispatchResult::Stop to abort the chain with a failure-style
// short-circuit. The dispatch driver treats Handled and Stop the same
// way from its own perspective (both terminate the walk); facades
// distinguish the two when they need to report back to the caller.
void AbstractMessageBus::dispatchChain(
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
        const auto result = deliver(slot, message);
        if (result == DispatchResult::Handled || result == DispatchResult::Stop)
        {
            return;
        }
        // Pass: fall through to the next subscriber.
    }
}

} // namespace vigine::messaging
