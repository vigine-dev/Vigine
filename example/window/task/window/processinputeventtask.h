#pragma once

#include <memory>
#include <vector>

#include "windoweventpayload.h"

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/messaging/isubscriber.h>
#include <vigine/api/messaging/isubscriptiontoken.h>
#include <vigine/api/messaging/routemode.h>

namespace vigine::messaging
{
class IMessage;
} // namespace vigine::messaging

/**
 * @brief Window-example task that receives input-event signals as an
 *        @ref vigine::messaging::ISubscriber.
 *
 * The task implements @ref vigine::messaging::ISubscriber so an
 * @ref vigine::messaging::ISignalEmitter can register it against
 * @ref kMouseButtonDownPayloadTypeId and @ref kKeyDownPayloadTypeId
 * directly. Subscription tokens are owned here through @ref
 * takeSubscriptionToken so the caller that performs the registrations
 * (for example @c main) does not need to keep a parallel vector alive.
 *
 * Destruction-order rule: @c _tokens is declared LAST among the data
 * members so that the reverse-declaration-order destruction contract
 * tears the subscriptions down BEFORE the handler state (@c _hasMouseEvent,
 * @c _hasKeyEvent) is destroyed. The destructor additionally calls
 * @c _tokens.clear() at its top as a belt-and-braces guarantee: if a
 * future member is added after @c _tokens, the explicit clear still runs
 * first and drains any in-flight @c onMessage before other fields go away.
 *
 * @c execute remains a no-op returning @c Success; the task participates
 * in the flow only to own its subscription tokens and serve as a
 * subscriber target.
 */
class ProcessInputEventTask final : public vigine::AbstractTask,
                                    public vigine::messaging::ISubscriber
{
  public:
    ProcessInputEventTask();
    ~ProcessInputEventTask() override;

    [[nodiscard]] vigine::Result execute() override;

    /**
     * @brief Delivers an incoming message from the bus to the matching
     *        private helper based on @ref vigine::messaging::IMessage::payloadTypeId.
     *
     * Dispatches on payload type id:
     *   - @ref kMouseButtonDownPayloadTypeId -> @c onMouseButtonDown.
     *   - @ref kKeyDownPayloadTypeId          -> @c onKeyDown.
     *
     * Both helper paths downcast the payload with @c dynamic_cast and
     * report @c DispatchResult::Handled on a successful match.
     * Unrecognised payload ids return @c DispatchResult::Pass so other
     * subscribers on a @c Chain- or @c FirstMatch-routed message still
     * get a chance to handle the envelope.
     */
    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage &message) override;

    /**
     * @brief Hands ownership of a subscription token to this task.
     *
     * The caller that registered the subscription (for example the
     * @c main entry point wiring the signal emitter to this subscriber)
     * passes the returned @c unique_ptr here so the task owns the token
     * for its lifetime. Tokens are released in the destructor before any
     * other handler state is torn down.
     *
     * A null token is silently ignored so callers can forward the raw
     * result of @ref vigine::messaging::ISignalEmitter::subscribeSignal
     * without a pre-check.
     */
    void takeSubscriptionToken(
        std::unique_ptr<vigine::messaging::ISubscriptionToken> token);

  private:
    void onMouseButtonDown(const MouseButtonDownPayload &payload);
    void onKeyDown(const KeyDownPayload &payload);

    bool _hasMouseEvent{false};
    bool _hasKeyEvent{false};

    // _tokens MUST stay as the last declared member. C++ destroys members
    // in reverse declaration order, so this placement guarantees the
    // subscription tokens are released (blocking on any in-flight
    // dispatch) before the handler state above goes away. The explicit
    // _tokens.clear() in the destructor is a belt-and-braces pair for
    // the same contract.
    std::vector<std::unique_ptr<vigine::messaging::ISubscriptionToken>> _tokens;
};
