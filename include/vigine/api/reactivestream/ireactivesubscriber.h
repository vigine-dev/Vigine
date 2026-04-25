#pragma once

#include <memory>

#include "vigine/api/reactivestream/ireactivesubscription.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/result.h"

namespace vigine::reactivestream
{

/**
 * @brief Pure-virtual consumer side of the reactive-streams contract.
 *
 * A subscriber registers itself with an @ref IReactiveStream via
 * @ref IReactiveStream::subscribe and receives a lifecycle sequence:
 *
 *   1. @ref onSubscribe is called exactly once, handing ownership of the
 *      @ref IReactiveSubscription to the subscriber. The subscriber
 *      stores the subscription and calls @ref IReactiveSubscription::request
 *      to signal its initial demand.
 *   2. @ref onNext is called at most once per demand unit, carrying each
 *      payload transferred from the publisher.
 *   3. Either @ref onComplete or @ref onError terminates the stream.
 *      No further calls happen after either terminal signal.
 *
 * Ownership: each @ref onNext invocation transfers ownership of the
 * payload @c unique_ptr to the subscriber; the subscriber is responsible
 * for its lifetime after that call returns.
 *
 * Thread-safety: the caller (ReactiveStream's delivery path)
 * serialises calls; the subscriber implementation does not need to be
 * re-entrant with respect to this subscriber instance.
 *
 * Invariants:
 *   - INV-1 : no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface.
 *   - INV-11: no graph types in this header.
 */
class IReactiveSubscriber
{
  public:
    virtual ~IReactiveSubscriber() = default;

    /**
     * @brief Called once when the subscription is established.
     *
     * The subscriber must retain @p subscription and call
     * @ref IReactiveSubscription::request before any items will be
     * delivered.
     */
    virtual void onSubscribe(std::unique_ptr<IReactiveSubscription> subscription) = 0;

    /**
     * @brief Called for each item within the granted demand.
     *
     * Transfers ownership of @p payload to the subscriber.
     * Not called after @ref onComplete or @ref onError.
     */
    virtual void onNext(std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    /**
     * @brief Terminal error signal.
     *
     * @p error describes the failure that terminated the stream.
     * No further calls to this subscriber will follow.
     */
    virtual void onError(vigine::Result error) = 0;

    /**
     * @brief Terminal completion signal.
     *
     * The publisher exhausted its items. No further calls will follow.
     */
    virtual void onComplete() = 0;

    IReactiveSubscriber(const IReactiveSubscriber &)            = delete;
    IReactiveSubscriber &operator=(const IReactiveSubscriber &) = delete;
    IReactiveSubscriber(IReactiveSubscriber &&)                 = delete;
    IReactiveSubscriber &operator=(IReactiveSubscriber &&)      = delete;

  protected:
    IReactiveSubscriber() = default;
};

} // namespace vigine::reactivestream
