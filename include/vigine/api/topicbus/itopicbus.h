#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/topicbus/topicid.h"
#include "vigine/result.h"

namespace vigine::topicbus
{

/**
 * @brief Pure-virtual facade for the pub/sub Topic pattern.
 *
 * @ref ITopicBus is the Level-2 facade over
 * @ref vigine::messaging::IMessageBus for named publish/subscribe topics
 * (plan_17, R.3.3.1.3). It encapsulates four operations:
 *
 *   - @ref createTopic  -- register a named topic; returns a stable
 *                          @ref TopicId. Idempotent: a second call with
 *                          the same name returns the same id.
 *   - @ref topicByName  -- look up an existing topic by name without
 *                          creating it.
 *   - @ref publish      -- post a payload to every subscriber on a topic.
 *                          Posts a @ref vigine::messaging::MessageKind::TopicPublish
 *                          message with @ref vigine::messaging::RouteMode::FanOut.
 *   - @ref subscribe    -- register a subscriber for a given topic; returns
 *                          a @c std::unique_ptr<ISubscriptionToken> (FF-1).
 *
 * Topic lookup:
 *   - @ref createTopic with an empty @c name returns
 *     @ref vigine::Result::Code::Error.
 *   - Subscribing to a non-existent topic is valid; the topic is created
 *     lazily on the first @ref publish.
 *
 * Ownership:
 *   - @ref publish takes unique ownership of the payload.
 *   - @ref subscribe returns a @c std::unique_ptr<ISubscriptionToken>.
 *     Dropping it or calling @ref vigine::messaging::ISubscriptionToken::cancel
 *     tears the subscription slot down.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-9: factory @ref createTopicBus returns @c std::unique_ptr.
 *   - INV-10: @c I prefix for this pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 *   - FF-1: @ref subscribe returns @c std::unique_ptr<ISubscriptionToken>.
 */
class ITopicBus
{
  public:
    virtual ~ITopicBus() = default;

    /**
     * @brief Registers (or retrieves) a named topic and returns its stable id.
     *
     * Idempotent: two calls with identical @p name return the same
     * @ref TopicId. An empty @p name is rejected — the method
     * returns `TopicId{0}`, the reserved invalid sentinel. (The
     * method returns @ref TopicId, not @ref vigine::Result; the
     * previous doc mentioned a @ref vigine::Result::Code::Error
     * return path the signature does not provide.)
     */
    [[nodiscard]] virtual TopicId createTopic(std::string_view name) = 0;

    /**
     * @brief Looks up an existing topic by name.
     *
     * Returns @c std::nullopt when no topic with @p name has been registered
     * via @ref createTopic or implicitly created by a prior @ref publish.
     */
    [[nodiscard]] virtual std::optional<TopicId> topicByName(std::string_view name) const = 0;

    /**
     * @brief Posts @p payload to every subscriber currently registered on
     *        @p topic.
     *
     * Under the hood the facade posts a
     * @ref vigine::messaging::MessageKind::TopicPublish message with
     * @ref vigine::messaging::RouteMode::FanOut to the underlying bus.
     * The bus takes ownership of the payload.
     *
     * Returns @ref vigine::Result::Code::Error when @p topic is invalid
     * (zero value) or the underlying bus has shut down.
     */
    [[nodiscard]] virtual vigine::Result
        publish(TopicId topic, std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    /**
     * @brief Subscribes @p subscriber to messages on @p topic and returns
     *        the RAII token owning the slot.
     *
     * Subscribing to a @ref TopicId whose @c value is zero creates the
     * topic lazily (assigned a synthetic id). Dropping the returned token
     * or calling @ref vigine::messaging::ISubscriptionToken::cancel
     * removes the subscription.
     *
     * A null @p subscriber returns an inert token.
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribe(TopicId topic, vigine::messaging::ISubscriber *subscriber) = 0;

    /**
     * @brief Shuts down the topic bus.
     *
     * Cancels all subscriptions and rejects subsequent @ref publish and
     * @ref subscribe calls. Idempotent.
     */
    virtual vigine::Result shutdown() = 0;

    ITopicBus(const ITopicBus &)            = delete;
    ITopicBus &operator=(const ITopicBus &) = delete;
    ITopicBus(ITopicBus &&)                 = delete;
    ITopicBus &operator=(ITopicBus &&)      = delete;

  protected:
    ITopicBus() = default;
};

} // namespace vigine::topicbus
