#pragma once

#include <cstddef>

namespace vigine::reactivestream
{

/**
 * @brief Pure-virtual handle that brokers demand between a subscriber and
 *        a cold publisher.
 *
 * @ref IReactiveSubscription is the back-channel in the reactive-streams
 * contract. A subscriber receives one subscription from
 * @ref IReactiveStream::subscribe; it calls @ref request to announce how
 * many items it can accept ("demand") and @ref cancel to tear the
 * subscription down without waiting for @c onComplete.
 *
 * Ownership: the @c std::unique_ptr<IReactiveSubscription> returned by
 * @ref IReactiveStream::subscribe is owned exclusively by the subscriber.
 * Dropping it calls the destructor, which behaves identically to
 * @ref cancel (idempotent).
 *
 * Thread-safety: @ref request and @ref cancel are safe to call from any
 * thread. Both are idempotent with respect to repeated calls after the
 * subscription has been cancelled or completed.
 *
 * Invariants:
 *   - INV-1 : no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface.
 *   - INV-11: no graph types in this header.
 */
class IReactiveSubscription
{
  public:
    virtual ~IReactiveSubscription() = default;

    /**
     * @brief Signals demand for @p n additional items from the publisher.
     *
     * Adds @p n to the outstanding demand counter. The publisher will
     * call @ref IReactiveSubscriber::onNext at most @p n more times
     * before the next @ref request call.
     *
     * A value of @c std::numeric_limits<std::size_t>::max() is treated
     * as unbounded demand. @p n == @c 0 is a no-op (documented).
     */
    virtual void request(std::size_t n) noexcept = 0;

    /**
     * @brief Cancels this subscription.
     *
     * After @ref cancel returns, the publisher will not call
     * @ref IReactiveSubscriber::onNext, @ref IReactiveSubscriber::onError,
     * or @ref IReactiveSubscriber::onComplete for this subscription.
     * Idempotent: subsequent calls are safe no-ops.
     */
    virtual void cancel() noexcept = 0;

    IReactiveSubscription(const IReactiveSubscription &)            = delete;
    IReactiveSubscription &operator=(const IReactiveSubscription &) = delete;
    IReactiveSubscription(IReactiveSubscription &&)                 = delete;
    IReactiveSubscription &operator=(IReactiveSubscription &&)      = delete;

  protected:
    IReactiveSubscription() = default;
};

} // namespace vigine::reactivestream
