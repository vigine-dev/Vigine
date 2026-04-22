#pragma once

#include <memory>

#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/requestbus/ifuture.h"
#include "vigine/requestbus/requestconfig.h"
#include "vigine/result.h"
#include "vigine/topicbus/topicid.h"

namespace vigine::requestbus
{

/**
 * @brief Pure-virtual Level-2 facade for the request/response pattern.
 *
 * @ref IRequestBus unifies three interaction patterns on top of
 * @ref vigine::messaging::IMessageBus:
 *
 *   1. **Local RPC** -- loosely-coupled procedure call through message
 *      passing.
 *   2. **Request/Response** -- one sender, one responder, bounded reply
 *      window.
 *   3. **Future/Promise** -- asynchronous wait with ownership-transferring
 *      result delivery.
 *
 * Request flow:
 *   - Caller invokes @ref request with a @ref vigine::topicbus::TopicId,
 *     a payload, and an optional @ref RequestConfig.
 *   - The bus posts a @c MessageKind::TopicRequest to the underlying bus
 *     and returns a @ref IFuture.
 *   - A responder subscribed via @ref respondTo receives the message,
 *     processes it, and calls @ref respond with the matching
 *     @ref vigine::messaging::CorrelationId and a reply payload.
 *   - The bus routes the reply to the pending future; @ref IFuture::wait
 *     unblocks and transfers payload ownership to the caller.
 *
 * TTL / late-reply protection (UD-5, Q-MF4):
 *   - Each request carries a TTL computed from @ref RequestConfig. After
 *     the TTL expires the correlation id is invalidated and any
 *     arriving late reply is silently dropped (logged at debug).
 *   - Default TTL = @c RequestConfig::timeout * 2 when
 *     @ref RequestConfig::ttl is zero.
 *
 * Ownership:
 *   - @ref request takes unique ownership of the payload.
 *   - @ref respond takes unique ownership of the reply payload.
 *   - @ref respondTo returns a @c std::unique_ptr<ISubscriptionToken>
 *     (FF-1). Dropping the token removes the responder subscription.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-9: @ref createRequestBus returns @c std::unique_ptr.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IRequestBus
{
  public:
    virtual ~IRequestBus() = default;

    /**
     * @brief Posts a request to @p topic and returns a @ref IFuture
     *        that resolves when the matching responder replies.
     *
     * @param topic   The address of the responder(s). The bus posts a
     *                @c MessageKind::TopicRequest with
     *                @c RouteMode::FirstMatch so only one responder
     *                handles each request.
     * @param payload Payload transferred to the bus. Must not be null.
     * @param cfg     Per-request timeout and TTL settings. Zero TTL
     *                means "default = timeout * 2".
     *
     * Returns a non-null @ref IFuture on success. Returns null only
     * when the bus has been shut down or @p payload is null.
     */
    [[nodiscard]] virtual std::unique_ptr<IFuture>
        request(vigine::topicbus::TopicId                          topic,
                std::unique_ptr<vigine::messaging::IMessagePayload> payload,
                const RequestConfig                               &cfg = {}) = 0;

    /**
     * @brief Registers @p subscriber as the responder for @p topic.
     *
     * The subscriber's @ref vigine::messaging::ISubscriber::onMessage
     * is invoked with every @c MessageKind::TopicRequest directed at
     * @p topic. The responder must call @ref respond with the
     * @ref vigine::messaging::CorrelationId from the incoming message
     * and the reply payload.
     *
     * A null @p subscriber or a shut-down bus returns @c nullptr —
     * callers must null-check the returned pointer before using it.
     * (Previous revisions of this doc described an inert-token
     * return; the sibling facade subscribe surfaces return inert
     * tokens, but the request-bus responder path is stricter about
     * invalid input.)
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::messaging::ISubscriptionToken>
        respondTo(vigine::topicbus::TopicId                topic,
                  vigine::messaging::ISubscriber          *subscriber) = 0;

    /**
     * @brief Posts a reply for the correlation id @p corrId.
     *
     * Resolves the @ref IFuture waiting on @p corrId and transfers
     * ownership of @p payload to the caller's @ref IFuture::wait.
     * If @p corrId is unknown (expired or already replied) the call
     * is silently dropped and logged -- this is not a user error.
     *
     * @param corrId  Correlation id copied from the incoming
     *                @c IMessage::correlationId().
     * @param payload Reply payload. Must not be null.
     */
    virtual void respond(vigine::messaging::CorrelationId                   corrId,
                         std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    /**
     * @brief Shuts down the request bus.
     *
     * Cancels every pending future, removes all responder subscriptions,
     * and rejects subsequent @ref request / @ref respondTo calls.
     * Idempotent.
     */
    virtual vigine::Result shutdown() = 0;

    IRequestBus(const IRequestBus &)            = delete;
    IRequestBus &operator=(const IRequestBus &) = delete;
    IRequestBus(IRequestBus &&)                 = delete;
    IRequestBus &operator=(IRequestBus &&)      = delete;

  protected:
    IRequestBus() = default;
};

} // namespace vigine::requestbus
