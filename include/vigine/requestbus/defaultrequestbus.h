#pragma once

#include <memory>

#include "vigine/requestbus/abstractrequestbus.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::requestbus
{

/**
 * @brief Concrete final request-bus facade.
 *
 * @ref DefaultRequestBus is Level-5 of the five-layer wrapper recipe.
 * It provides the full @ref IRequestBus implementation on top of
 * @ref AbstractRequestBus:
 *
 *   - An atomic @c uint64_t counter stamps each request with a unique
 *     @ref vigine::messaging::CorrelationId.
 *   - A mutex-guarded @c unordered_map maps correlation ids to
 *     @c FuturePromise shared-state objects.
 *   - An internal @ref vigine::messaging::ISubscriber listens for
 *     @c MessageKind::TopicPublish with known correlation ids and
 *     resolves the matching promise.
 *   - TTL cleanup: after the effective TTL (UD-5 configurable, default
 *     @c timeout * 2) a task posted via @ref vigine::core::threading::IThreadManager
 *     removes the correlation id so late replies are dropped.
 *
 * Callers obtain instances exclusively through @ref createRequestBus --
 * they never construct this type by name.
 *
 * Thread-safety: @ref request, @ref respond, @ref respondTo, and
 * @ref shutdown are safe to call from any thread concurrently. The
 * correlation map is guarded by a @c std::mutex.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref createRequestBus returns @c std::unique_ptr<IRequestBus>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultRequestBus final : public AbstractRequestBus
{
  public:
    /**
     * @brief Constructs the request-bus facade over @p bus.
     *
     * @p bus and @p threadManager must outlive this facade instance.
     */
    DefaultRequestBus(vigine::messaging::IMessageBus           &bus,
                      vigine::core::threading::IThreadManager        &threadManager);

    ~DefaultRequestBus() override;

    // IRequestBus
    [[nodiscard]] std::unique_ptr<IFuture>
        request(vigine::topicbus::TopicId                           topic,
                std::unique_ptr<vigine::messaging::IMessagePayload> payload,
                const RequestConfig                                &cfg = {}) override;

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        respondTo(vigine::topicbus::TopicId               topic,
                  vigine::messaging::ISubscriber          *subscriber) override;

    void respond(vigine::messaging::CorrelationId                    corrId,
                 std::unique_ptr<vigine::messaging::IMessagePayload> payload) override;

    vigine::Result shutdown() override;

    DefaultRequestBus(const DefaultRequestBus &)            = delete;
    DefaultRequestBus &operator=(const DefaultRequestBus &) = delete;
    DefaultRequestBus(DefaultRequestBus &&)                 = delete;
    DefaultRequestBus &operator=(DefaultRequestBus &&)      = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Factory function -- the sole entry point for creating a
 *        request-bus facade.
 *
 * Returns a @c std::unique_ptr<IRequestBus> so the caller owns the
 * facade exclusively (FF-1, INV-9). Both @p bus and @p threadManager
 * must outlive the returned facade.
 */
[[nodiscard]] std::unique_ptr<IRequestBus>
    createRequestBus(vigine::messaging::IMessageBus    &bus,
                     vigine::core::threading::IThreadManager &threadManager);

} // namespace vigine::requestbus
