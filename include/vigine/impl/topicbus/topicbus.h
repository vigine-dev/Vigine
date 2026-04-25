#pragma once

#include <memory>

#include "vigine/api/topicbus/abstracttopicbus.h"

namespace vigine::topicbus
{

/**
 * @brief Concrete final topic-bus facade.
 *
 * @ref TopicBus is Level-5 of the five-layer wrapper recipe. It
 * provides the full @ref ITopicBus implementation on top of
 * @ref AbstractTopicBus: a hash-based topic registry for name-to-id mapping
 * and subscription management wired through the underlying
 * @ref vigine::messaging::IMessageBus.
 *
 * Callers obtain instances exclusively through @ref createTopicBus -- they
 * never construct this type by name.
 *
 * Hash-based topic ids:
 *   - @ref createTopic hashes the name to produce a @ref TopicId. Collisions
 *     are resolved with a chained-list inside the registry so every name maps
 *     to a unique stable id. Zero is reserved as the invalid sentinel.
 *
 * Thread-safety: @ref createTopic, @ref topicByName, @ref publish,
 * @ref subscribe, and @ref shutdown are safe to call from any thread
 * concurrently. The topic registry is guarded by a @c std::shared_mutex.
 * The underlying bus provides its own thread-safety for @ref post and
 * @ref subscribe.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref subscribe returns @c std::unique_ptr<ISubscriptionToken>.
 *   - INV-11: no graph types leak into this header.
 */
class TopicBus final : public AbstractTopicBus
{
  public:
    /**
     * @brief Constructs the topic-bus facade over @p bus.
     *
     * @p bus must outlive this facade instance.
     */
    explicit TopicBus(vigine::messaging::IMessageBus &bus);

    ~TopicBus() override;

    // ITopicBus
    [[nodiscard]] TopicId createTopic(std::string_view name) override;
    [[nodiscard]] std::optional<TopicId> topicByName(std::string_view name) const override;
    [[nodiscard]] vigine::Result
        publish(TopicId                                            topic,
                std::unique_ptr<vigine::messaging::IMessagePayload> payload) override;
    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribe(TopicId topic, vigine::messaging::ISubscriber *subscriber) override;
    vigine::Result shutdown() override;

    TopicBus(const TopicBus &)            = delete;
    TopicBus &operator=(const TopicBus &) = delete;
    TopicBus(TopicBus &&)                  = delete;
    TopicBus &operator=(TopicBus &&)       = delete;

  private:
    // Pimpl hides the topic registry and subscription tokens so private
    // implementation details are not exposed in the public header.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::topicbus
