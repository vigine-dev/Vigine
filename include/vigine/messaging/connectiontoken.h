#pragma once

#include <atomic>
#include <memory>

#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/iconnectiontoken.h"
#include "vigine/messaging/subscriptionslot.h"

namespace vigine::messaging
{
class IBusControlBlock;

/**
 * @brief Concrete RAII token bound to a single bus-side connection slot.
 *
 * @ref ConnectionToken is the only shipped implementation of
 * @ref IConnectionToken. It carries a @c std::weak_ptr to the bus's
 * @ref IBusControlBlock so that it tolerates the bus being destroyed
 * first, the @ref ConnectionId that addresses its slot inside that
 * block, and a @c std::shared_ptr to the slot's @ref SlotState so the
 * cancel barrier can run without reaching back into the registry. The
 * destructor enforces the strong-unsubscribe barrier in two
 * cooperating steps: it asks the control block to unregister the slot
 * (which captures the same @ref SlotState and acquires
 * @c lifecycleMutex exclusively, draining every concurrent
 * @c onMessage), and it then trips @c cancelled to a steady-state
 * @c true so that any snapshot copy of the slot still in flight
 * observes the flag and skips @c onMessage. That sequence is what
 * keeps the target's lifetime strictly longer than any dispatch that
 * could still be running against it.
 *
 * A token is non-copyable and non-movable: the RAII contract ties one
 * token to one slot for its entire lifetime.
 *
 * Thread-safety: construction, @ref active, and destruction are safe to
 * call from any thread. The destructor is permitted to block until
 * in-flight dispatches on this slot complete, which is the whole point
 * of the strong-unsubscribe guarantee.
 *
 * Reentrancy: a target's @c onMessage implementation must not destroy
 * the token (or the enclosing target) while the call is in flight.
 * Doing so would make the destructor wait for itself -- a self-deadlock.
 * Implementations document this as a hard precondition; runtime
 * detection is out of scope for this leaf.
 */
class ConnectionToken final : public IConnectionToken
{
  public:
    /**
     * @brief Builds a token bound to the slot addressed by @p id inside
     *        the control block reachable through @p control, sharing
     *        @p slotState with the registry and the dispatch path.
     *
     * The @c std::weak_ptr lets the token tolerate the bus being
     * destroyed before the target. @p id is stored as-is; invalid ids
     * (generation zero) produce a token whose @ref active always
     * reports @c false. @p slotState is the @c shared_ptr returned by
     * @ref IBusControlBlock::allocateSlot inside @ref SlotAllocation;
     * it MUST refer to the same live object that the registry slot
     * holds. Passing an empty @c shared_ptr produces a token whose
     * destructor still runs @c unregisterTarget but skips the cancel
     * barrier — matching the inert-token shape that callers see when
     * allocation failed.
     */
    ConnectionToken(std::weak_ptr<IBusControlBlock> control,
                    ConnectionId                   id,
                    std::shared_ptr<SlotState>     slotState) noexcept;

    /**
     * @brief Unregisters the slot (if the bus is still alive) and then
     *        flips @c slotState->cancelled under the exclusive
     *        @c lifecycleMutex so every still-in-flight dispatch on
     *        this slot drains before this destructor returns.
     *
     * Delegates to @ref cancel so the RAII teardown path and the
     * explicit-cancel path share one body. The destructor is the exact
     * mirror of @ref AbstractMessageBus::SubscriptionToken::cancel:
     * lock the weak_ptr to the control block, call @c unregisterTarget
     * on a live block (the block itself drives the cancel barrier
     * through the slot's @c lifecycleMutex), and finally — defensively —
     * acquire the same @c lifecycleMutex exclusively from the token
     * side and trip @c cancelled. The defensive step covers the path
     * where the bus died first (weak_ptr.lock() returns null) and the
     * registry was reclaimed en masse: the token still owns its share
     * of the @ref SlotState and any racing dispatch holding a snapshot
     * of the same state still observes @c cancelled before it enters
     * the dispatch shared region.
     *
     * See the class-level note on reentrancy: destroying a token from
     * inside its own dispatch causes a self-deadlock.
     */
    ~ConnectionToken() override;

    /**
     * @brief Explicitly releases the slot ahead of destruction.
     *
     * Runs the same unregister-then-barrier sequence as the
     * destructor. Idempotent across repeated calls via the
     * @c _cancelled atomic exchange; the second caller short-circuits
     * before touching the control block or the @c lifecycleMutex so a
     * repeat is wait-free. Blocks on the slot's @c lifecycleMutex when
     * a dispatch is in flight, matching the dtor-blocks contract the
     * @ref IConnectionToken header documents.
     */
    void cancel() override;

    /**
     * @brief Returns @c true only when (a) the id is valid, (b) this
     *        token's own @c _cancelled atomic is still @c false, (c)
     *        the shared @ref SlotState reports @c cancelled == @c false
     *        (so a sibling unregister path that flipped the slot
     *        without going through this token's @ref cancel still
     *        retires @ref active here), and (d) the control block is
     *        reachable and @ref IBusControlBlock::isAlive reports
     *        @c true.
     *
     * The middle SlotState check is what makes @ref active honour an
     * explicit unregister driven from outside the token: the bus may
     * call @ref IBusControlBlock::unregisterTarget on its own (e.g.
     * programmatic teardown of a target's slots) which flips
     * @c slotState->cancelled under @c lifecycleMutex; once that
     * happens the slot is gone from the registry and any caller still
     * holding the token must observe @ref active as @c false even
     * before the bus itself goes dead.
     *
     * Cheap: one valid-id check, one atomic load on @c _cancelled,
     * one direct (lock-free) read of @c _slotState->cancelled, one
     * @c weak_ptr::lock, one atomic load on the bus alive flag. The
     * return value is a momentary snapshot; callers racing with
     * @ref IBusControlBlock::markDead, a concurrent @ref cancel, or a
     * sibling unregister path may observe the transition between the
     * check and any follow-up operation.
     */
    [[nodiscard]] bool active() const noexcept override;

    /**
     * @brief Returns the connection id addressing this token's slot.
     *
     * Stable for the lifetime of the token. Safe to call concurrently
     * with any other member function.
     */
    [[nodiscard]] ConnectionId id() const noexcept { return _id; }

    ConnectionToken(const ConnectionToken &)            = delete;
    ConnectionToken &operator=(const ConnectionToken &) = delete;
    ConnectionToken(ConnectionToken &&)                 = delete;
    ConnectionToken &operator=(ConnectionToken &&)      = delete;

  private:
    std::weak_ptr<IBusControlBlock> _control;
    ConnectionId                    _id;
    // Shared with the registry slot and every dispatch snapshot. The
    // destructor acquires `_slotState->lifecycleMutex` exclusively and
    // sets `_slotState->cancelled` to drain every concurrent
    // onMessage; mirrors the SubscriptionToken cancel path.
    std::shared_ptr<SlotState>      _slotState;
    // Idempotency flag for `cancel`. The first caller (explicit or
    // the destructor) wins the compare_exchange and runs the full
    // unregister-plus-barrier sequence; later callers short-circuit
    // before touching either the control block or the lifecycle
    // mutex. Mirrors the same role `_cancelled` plays inside
    // `AbstractMessageBus::SubscriptionToken`.
    std::atomic<bool>               _cancelled{false};
};

} // namespace vigine::messaging
