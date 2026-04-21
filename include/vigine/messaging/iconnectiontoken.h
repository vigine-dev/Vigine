#pragma once

namespace vigine::messaging
{
/**
 * @brief Pure-virtual handle to a single subscription slot on an
 *        @ref IBusControlBlock.
 *
 * @ref IConnectionToken is the subscriber-facing surface returned by an
 * @ref IBusControlBlock after @ref IBusControlBlock::allocateSlot. Its
 * only semantic contract is that @ref active reports whether the backing
 * bus and slot are still live; the concrete RAII token
 * (@ref ConnectionToken) adds destructor-driven unregistration, weak
 * tracking of the control block, and a strong-unsubscribe barrier.
 *
 * Ownership: tokens are owned by their target (typically by
 * @ref AbstractMessageTarget holding them in its @c _connections vector).
 * The bus never owns the token; its only reference is through the
 * registry slot, which becomes inert once the token's destructor runs.
 *
 * Thread-safety: @ref active is safe to call from any thread at any
 * time. Implementations must keep the call wait-free on the fast path.
 */
class IConnectionToken
{
  public:
    virtual ~IConnectionToken() = default;

    /**
     * @brief Returns @c true when the subscription slot is still live.
     *
     * A token becomes inactive when the underlying bus is marked dead
     * (either through @ref IBusControlBlock::markDead or the bus being
     * destroyed) or when the token's slot has been explicitly
     * unregistered. Once @ref active returns @c false, it never returns
     * @c true again.
     */
    [[nodiscard]] virtual bool active() const noexcept = 0;

    IConnectionToken(const IConnectionToken &)            = delete;
    IConnectionToken &operator=(const IConnectionToken &) = delete;
    IConnectionToken(IConnectionToken &&)                 = delete;
    IConnectionToken &operator=(IConnectionToken &&)      = delete;

  protected:
    IConnectionToken() = default;
};

} // namespace vigine::messaging
