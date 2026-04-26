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
 * tracking of the control block, and a strong-unsubscribe barrier
 * driven through the slot's @c lifecycleMutex.
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
     * @brief Tears the connection slot down immediately.
     *
     * One-shot release of the connection: asks the underlying bus
     * control block to retire the registry slot and trips the
     * cancellation flag on the shared @ref SlotState so every
     * still-in-flight dispatch on this slot drains before @ref cancel
     * returns.
     *
     * Idempotent: a second call is a no-op, and calling @ref cancel
     * after the destructor has already run (on a concrete
     * @ref ConnectionToken that already unregistered itself) is
     * structurally impossible because the token is gone -- but a
     * repeated @ref cancel on a still-live token is safely a no-op.
     *
     * Blocks if a dispatch is in flight on this slot: @ref cancel
     * acquires the slot's @c lifecycleMutex exclusively, which waits
     * for every shared-holder (every concurrent @c onMessage) to
     * release. This is the same dtor-blocks contract the destructor
     * honours; explicit @ref cancel and RAII teardown share the same
     * barrier.
     *
     * Mirrors @ref ISubscriptionToken::cancel for API symmetry: a
     * caller that needs to drop a target's connection earlier than
     * the owning target's destructor may do so without destroying the
     * token storage.
     */
    virtual void cancel() = 0;

    /**
     * @brief Returns @c true when the subscription slot is still live.
     *
     * A token becomes inactive when any of the following happen:
     *  - @ref cancel is called on the token (the same atomic flag
     *    that drives the unregister-and-barrier sequence is also
     *    consulted here, so once @ref cancel has run @ref active
     *    must report @c false);
     *  - the slot is explicitly unregistered out-of-band (e.g. the
     *    bus drives @c unregisterTarget on its own and flips the
     *    shared @ref SlotState's @c cancelled flag without going
     *    through this token's @ref cancel) — implementations MUST
     *    consult that flag in addition to their own internal one;
     *  - the underlying bus is marked dead (either through
     *    @ref IBusControlBlock::markDead or the bus being destroyed).
     *
     * Once @ref active returns @c false it never returns @c true
     * again.
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
