#pragma once

#include <memory>

#include "vigine/reactivestream/ireactivesubscriber.h"
#include "vigine/reactivestream/ireactivesubscription.h"
#include "vigine/result.h"

namespace vigine::reactivestream
{

/**
 * @brief Pure-virtual Level-2 facade for the reactive-streams pattern.
 *
 * @ref IReactiveStream is the entry point for the cold-publisher /
 * backpressure-aware reactive pipeline built on top of
 * @ref vigine::messaging::IMessageBus.
 *
 * Reactive-streams contract:
 *   - @ref subscribe wires a fresh, independent publisher to @p subscriber
 *     (cold publisher model: every subscriber starts its own stream).
 *   - The returned @ref IReactiveSubscription lets the subscriber
 *     control demand via @ref IReactiveSubscription::request.
 *   - The bus delivers items on the subscriber's demand; no item is
 *     pushed beyond the declared demand.
 *   - Terminal signals (@c onComplete / @c onError) are sent once;
 *     after either the subscription is closed.
 *
 * Backpressure:
 *   - Outstanding demand is accumulated; the publisher respects it.
 *   - @ref IReactiveSubscription::request(0) is a no-op.
 *   - @ref IReactiveSubscription::request(std::numeric_limits<std::size_t>::max())
 *     acts as unbounded demand.
 *
 * Ownership:
 *   - @ref subscribe returns a non-null @c std::unique_ptr<IReactiveSubscription>
 *     on success; the subscriber owns it (FF-1 / INV-9).
 *   - A null @p subscriber returns a null unique_ptr (invalid subscription).
 *
 * Thread-safety: @ref subscribe and @ref shutdown are safe to call from
 * any thread.
 *
 * Invariants:
 *   - INV-1 : no template parameters in the public surface.
 *   - INV-9 : @ref subscribe returns @c std::unique_ptr.
 *   - INV-10: @c I prefix for a pure-virtual interface.
 *   - INV-11: no graph types in this header.
 */
class IReactiveStream
{
  public:
    virtual ~IReactiveStream() = default;

    /**
     * @brief Subscribes @p subscriber to this stream.
     *
     * Creates a new independent cold publisher, calls
     * @ref IReactiveSubscriber::onSubscribe with the subscription token,
     * and begins delivery once the subscriber calls
     * @ref IReactiveSubscription::request.
     *
     * A null @p subscriber returns a null @c unique_ptr.
     * A call after @ref shutdown returns a null @c unique_ptr.
     *
     * @param subscriber  Non-owning pointer to the subscriber. Must
     *                    outlive the returned subscription token.
     * @return Ownership of the subscription RAII handle, or null.
     */
    [[nodiscard]] virtual std::unique_ptr<IReactiveSubscription>
        subscribe(IReactiveSubscriber *subscriber) = 0;

    /**
     * @brief Shuts down the stream.
     *
     * Cancels every active subscription, signals @c onComplete to
     * each subscriber, and rejects subsequent @ref subscribe calls.
     * Idempotent.
     */
    virtual vigine::Result shutdown() = 0;

    IReactiveStream(const IReactiveStream &)            = delete;
    IReactiveStream &operator=(const IReactiveStream &) = delete;
    IReactiveStream(IReactiveStream &&)                 = delete;
    IReactiveStream &operator=(IReactiveStream &&)      = delete;

  protected:
    IReactiveStream() = default;
};

} // namespace vigine::reactivestream
