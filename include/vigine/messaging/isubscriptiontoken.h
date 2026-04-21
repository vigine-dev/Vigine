#pragma once

namespace vigine::messaging
{
/**
 * @brief Pure-virtual RAII handle returned by @ref IMessageBus::subscribe.
 *
 * The token owns one subscription slot. Dropping the token (or calling
 * @ref cancel) tears the slot down: the bus stops delivering further
 * messages and any in-flight dispatch on the slot is allowed to drain.
 * The destructor is the canonical way to unsubscribe; @ref cancel exists
 * so callers can drop the subscription earlier without freeing the
 * token storage.
 *
 * Ownership: tokens are handed out as @c std::unique_ptr so the RAII
 * contract is visible at every call site. Callers may relocate the
 * @c unique_ptr freely; the underlying token identity does not move.
 *
 * Thread-safety: @ref cancel and @ref active are safe to call from any
 * thread. The destructor blocks until every in-flight dispatch targeting
 * this slot has returned -- mirroring the strong-unsubscribe guarantee
 * that @ref ConnectionToken provides for registered @ref
 * AbstractMessageTarget slots.
 */
class ISubscriptionToken
{
  public:
    virtual ~ISubscriptionToken() = default;

    /**
     * @brief Tears the subscription slot down immediately.
     *
     * Idempotent: a second call is a no-op. After @ref cancel returns,
     * @ref active reports @c false and the destructor no longer has
     * anything to tear down.
     */
    virtual void cancel() noexcept = 0;

    /**
     * @brief Returns @c true when the subscription slot is still live.
     *
     * Reports @c false after @ref cancel or after the bus has been
     * shut down. Once @ref active returns @c false, it never returns
     * @c true again.
     */
    [[nodiscard]] virtual bool active() const noexcept = 0;

    ISubscriptionToken(const ISubscriptionToken &)            = delete;
    ISubscriptionToken &operator=(const ISubscriptionToken &) = delete;
    ISubscriptionToken(ISubscriptionToken &&)                 = delete;
    ISubscriptionToken &operator=(ISubscriptionToken &&)      = delete;

  protected:
    ISubscriptionToken() = default;
};

} // namespace vigine::messaging
