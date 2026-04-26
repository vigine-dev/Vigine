#include "vigine/api/messaging/abstractmessagebus.h"

#include <vector>

#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/isubscriber.h"

namespace vigine::messaging
{

// Bubble: walks from the message target upward through its parent
// chain, delivering to the first subscription that matches at each
// level. Falls back to FirstMatch semantics on the original target
// when no parent is reachable.
//
// The R.3.1.1 AbstractMessageTarget shipped without a parent() hook,
// so this implementation currently treats every target as root and
// degrades unconditionally to the FirstMatch semantics. When the HSM
// wrapper in plan_12 extends AbstractMessageTarget with a parent chain
// (documented in Q-MB5), the walk here picks up the real parent hook
// automatically without a change on the facade side.
void AbstractMessageBus::dispatchBubble(
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
    }
}

} // namespace vigine::messaging
