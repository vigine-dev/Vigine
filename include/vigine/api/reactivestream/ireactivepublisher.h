#pragma once

#include <memory>

#include "vigine/api/reactivestream/ireactivesubscriber.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/result.h"

namespace vigine::reactivestream
{

/**
 * @brief Pure-virtual producer side of the reactive-streams contract.
 *
 * An @ref IReactivePublisher emits items to its paired
 * @ref IReactiveSubscriber bounded by the subscriber's outstanding demand.
 * Once a publisher is wired to a subscriber (via
 * @ref IReactiveStream::subscribe), the delivery protocol is:
 *
 *   - @ref onNext is called at most @c demand times per batch, where
 *     @c demand accumulates from @ref IReactiveSubscription::request calls.
 *   - After the publisher exhausts its sequence it calls @ref onComplete.
 *   - If a non-recoverable error occurs it calls @ref onError instead;
 *     @ref onComplete is then suppressed.
 *
 * Each @ref IReactiveStream::subscribe creates a fresh, independent
 * publisher instance (cold publisher model): two subscribers subscribing
 * to the same stream share no publisher state.
 *
 * Thread-safety: once wired, the publisher is driven exclusively by the
 * bus dispatch path and is not re-entrant with itself.
 *
 * Invariants:
 *   - INV-1 : no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface.
 *   - INV-11: no graph types in this header.
 */
class IReactivePublisher
{
  public:
    virtual ~IReactivePublisher() = default;

    /**
     * @brief Delivers one item to the subscriber.
     *
     * Transfers ownership of @p payload. Only called while demand > 0.
     * Not called after @ref onComplete or @ref onError.
     */
    virtual void onNext(std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    /**
     * @brief Signals a terminal error to the subscriber.
     *
     * @p error describes why the stream terminated abnormally.
     * No further callbacks happen after this call returns.
     */
    virtual void onError(vigine::Result error) = 0;

    /**
     * @brief Signals normal end-of-stream to the subscriber.
     *
     * No further callbacks happen after this call returns.
     */
    virtual void onComplete() = 0;

    IReactivePublisher(const IReactivePublisher &)            = delete;
    IReactivePublisher &operator=(const IReactivePublisher &) = delete;
    IReactivePublisher(IReactivePublisher &&)                 = delete;
    IReactivePublisher &operator=(IReactivePublisher &&)      = delete;

  protected:
    IReactivePublisher() = default;
};

} // namespace vigine::reactivestream
