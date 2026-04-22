#pragma once

#include <memory>

#include "vigine/reactivestream/abstractreactivestream.h"
#include "vigine/reactivestream/ireactivesubscription.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::reactivestream
{

/**
 * @brief Concrete final reactive-stream facade.
 *
 * @ref DefaultReactiveStream is Level-5 of the five-layer wrapper recipe.
 * It provides the full @ref IReactiveStream implementation on top of
 * @ref AbstractReactiveStream:
 *
 *   - Cold publisher model: @ref subscribe creates a fresh, independent
 *     @ref IReactiveSubscription (and publisher) for each caller. Two
 *     subscribers on the same stream never share delivery state.
 *   - Backpressure: demand is tracked per subscription; the publisher
 *     calls @c onNext only while demand > 0.
 *   - @ref IReactiveSubscription::cancel is idempotent; the subscriber's
 *     @c onNext stops immediately after cancel returns.
 *   - Terminal signals (@c onComplete / @c onError) mark the subscription
 *     closed; the delivery path is a no-op afterwards.
 *   - @ref shutdown drains in-flight delivery, cancels every subscription
 *     with @c onComplete, and rejects new @ref subscribe calls. Idempotent.
 *
 * Callers obtain instances exclusively through @ref createReactiveStream;
 * they never construct this type by name.
 *
 * Thread-safety: @ref subscribe and @ref shutdown are safe to call from
 * any thread concurrently. The subscription registry is guarded by a
 * @c std::mutex.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref createReactiveStream returns @c std::unique_ptr<IReactiveStream>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultReactiveStream final : public AbstractReactiveStream
{
  public:
    /**
     * @brief Constructs the reactive-stream facade over @p bus.
     *
     * @p bus and @p threadManager must outlive this facade instance.
     */
    DefaultReactiveStream(vigine::messaging::IMessageBus    &bus,
                          vigine::threading::IThreadManager &threadManager);

    ~DefaultReactiveStream() override;

    // IReactiveStream
    [[nodiscard]] std::unique_ptr<IReactiveSubscription>
        subscribe(IReactiveSubscriber *subscriber) override;

    vigine::Result shutdown() override;

    /**
     * @brief Pushes @p payload to every active subscriber with outstanding
     *        demand.
     *
     * Called by the engine (or tests) to deliver an item to all subscribers
     * that have signalled demand via @ref IReactiveSubscription::request.
     * Subscribers with zero demand do not receive the item (backpressure).
     *
     * @p payload ownership is transferred to the first subscriber that
     * accepts it. When multiple subscribers share the stream, only the
     * first subscriber with non-zero demand receives the payload in this
     * simplified implementation. Engine code that needs fan-out to all
     * subscribers must post a payload per subscriber.
     *
     * Returns @c Result::Code::Success even when no subscriber has demand
     * (the item is silently dropped in that case, consistent with
     * backpressure semantics).
     */
    vigine::Result publish(std::unique_ptr<vigine::messaging::IMessagePayload> payload);

    /**
     * @brief Signals @c onComplete to every active subscriber.
     *
     * Marks each subscription terminal. Idempotent per subscription.
     */
    vigine::Result complete();

    /**
     * @brief Signals @c onError(@p error) to every active subscriber.
     *
     * Marks each subscription terminal. Idempotent per subscription.
     */
    vigine::Result fail(vigine::Result error);

    DefaultReactiveStream(const DefaultReactiveStream &)            = delete;
    DefaultReactiveStream &operator=(const DefaultReactiveStream &) = delete;
    DefaultReactiveStream(DefaultReactiveStream &&)                 = delete;
    DefaultReactiveStream &operator=(DefaultReactiveStream &&)      = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Factory function — the sole entry point for creating a
 *        reactive-stream facade.
 *
 * Returns a @c std::unique_ptr<IReactiveStream> so the caller owns the
 * facade exclusively (FF-1, INV-9). Both @p bus and @p threadManager
 * must outlive the returned facade.
 */
[[nodiscard]] std::unique_ptr<IReactiveStream>
    createReactiveStream(vigine::messaging::IMessageBus    &bus,
                         vigine::threading::IThreadManager &threadManager);

} // namespace vigine::reactivestream
