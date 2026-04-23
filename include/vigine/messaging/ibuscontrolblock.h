#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/subscriptionslot.h"

namespace vigine::messaging
{
class AbstractMessageTarget;
class ISubscriber;

/**
 * @brief Pure-virtual shared-heap state owned jointly by a message bus
 *        and the connection tokens it hands out.
 *
 * @ref IBusControlBlock is the safety anchor that removes the
 * destruction-ordering hazard between an @ref IMessageBus and the two
 * kinds of handles it hands out: @ref ConnectionToken for registered
 * @ref AbstractMessageTarget objects and
 * @ref AbstractMessageBus::SubscriptionToken for subscribers. The bus
 * holds a @c std::shared_ptr to the control block and every token holds
 * a @c std::weak_ptr to the same block. When the bus dies first, its
 * destructor calls @ref markDead so that every still-held token becomes
 * a safe no-op on destruction; when a target or subscriber dies first,
 * the token's destructor calls @ref unregisterTarget or
 * @ref unregisterSubscription so the matching registry never sees a
 * dangling pointer.
 *
 * The block carries BOTH registries: a target registry that resolves
 * @ref ConnectionId values back to live @ref AbstractMessageTarget
 * pointers for outbound dispatch, and a subscription registry that the
 * bus snapshots on the dispatch hot path to reach subscribers. Keeping
 * both inside the control block is how a token can erase its own entry
 * without ever touching the bus pointer — which is what makes the
 * bus-destroyed-first race safe for subscribers the same way it is for
 * targets.
 *
 * Ownership and lifetime:
 *   - The control block outlives the bus exactly long enough for any
 *     remaining weak tokens to be destroyed cleanly. It is always
 *     allocated on the heap and only reached through a shared/weak
 *     pointer pair.
 *   - @ref allocateSlot records a raw, non-owning pointer to the target.
 *     The caller (the bus) must guarantee that the target outlives the
 *     corresponding token or that the token's destructor runs before the
 *     target is freed -- which is the invariant provided by
 *     @ref AbstractMessageTarget owning its tokens.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 * Implementations typically protect the registry with a
 * @c std::shared_mutex and the alive flag with a @c std::atomic.
 */
class IBusControlBlock
{
  public:
    virtual ~IBusControlBlock() = default;

    /**
     * @brief Returns @c true when the bus is still alive.
     *
     * Reads must be cheap and wait-free. Implementations typically back
     * this with a @c std::atomic<bool> loaded with acquire ordering.
     */
    [[nodiscard]] virtual bool isAlive() const noexcept = 0;

    /**
     * @brief Marks the bus as dead.
     *
     * Called from the bus destructor. Idempotent: a second call is a
     * no-op. After @ref markDead returns, every future call to
     * @ref isAlive reports @c false and every future call to
     * @ref unregisterTarget is a no-op, regardless of the connection id.
     */
    virtual void markDead() noexcept = 0;

    /**
     * @brief Allocates a new registry slot for @p target and returns its
     *        generational id.
     *
     * The returned @ref ConnectionId always has a non-zero @c generation
     * when allocation succeeded. When the bus is already dead or when
     * the registry is exhausted, implementations return a default-
     * constructed sentinel (@c generation == 0) so that call sites can
     * detect the failure without needing a separate @ref Result.
     *
     * Passing @c nullptr is a programming error; implementations return
     * the sentinel id and do not insert anything into the registry.
     */
    [[nodiscard]] virtual ConnectionId
        allocateSlot(AbstractMessageTarget *target) = 0;

    /**
     * @brief Removes the registry entry addressed by @p id.
     *
     * Idempotent: unregistering a stale id or an id from a dead bus is a
     * no-op. After this call returns, no future dispatch on this bus
     * reaches the slot addressed by @p id; in-flight dispatches on that
     * slot are serialised separately by the token (see
     * @ref ConnectionToken).
     */
    virtual void unregisterTarget(ConnectionId id) noexcept = 0;

    /**
     * @brief Registers a subscription slot and returns its serial id.
     *
     * Called by the message bus from inside
     * @ref IMessageBus::subscribe. The control block assigns a
     * monotonic, non-zero serial, stores a @ref SubscriptionSlot built
     * from the arguments, and hands the serial back so the caller can
     * pair it with a @c std::weak_ptr to this block inside the
     * @ref ISubscriptionToken it returns to the user.
     *
     * The block stores @p subscriber as a raw, non-owning pointer; the
     * caller (the bus) must guarantee that either the subscriber
     * outlives its token or the token's destructor runs before the
     * subscriber is freed -- the same invariant that
     * @ref ISubscriptionToken's dtor-blocks contract provides.
     *
     * Returns @c 0 (the inert sentinel) when:
     *   - the block is already dead (after @ref markDead),
     *   - @p subscriber is @c nullptr, or
     *   - @p slotState is empty.
     *
     * Tokens built with an inert serial become no-ops for every
     * subsequent @ref ISubscriptionToken operation, so call sites do
     * not need a separate @ref Result to signal failure.
     */
    [[nodiscard]] virtual std::uint64_t
        registerSubscription(ISubscriber               *subscriber,
                             MessageFilter              filter,
                             std::shared_ptr<SlotState> slotState) = 0;

    /**
     * @brief Removes the subscription addressed by @p serial.
     *
     * Idempotent: unregistering an unknown serial, the inert serial
     * (@c 0), or a serial on a dead bus is a no-op. On a live bus the
     * entry is erased from the subscription registry AND the slot's
     * @ref SlotState is driven through the exclusive @c lifecycleMutex
     * so every still-in-flight dispatch drains before this call
     * returns. Snapshot copies that have not yet entered the dispatch
     * shared region observe the @c cancelled flag and skip
     * @c onMessage.
     *
     * Called from @ref AbstractMessageBus::SubscriptionToken's cancel
     * path after locking the @c weak_ptr on this block.
     */
    virtual void unregisterSubscription(std::uint64_t serial) noexcept = 0;

    /**
     * @brief Returns a value-copy of every active subscription slot.
     *
     * Called by the bus on the dispatch hot path. The copy is taken
     * under an internal shared lock so a concurrent
     * @ref registerSubscription or @ref unregisterSubscription does
     * not trip the iteration. The snapshot's @c slotState pointers
     * still refer to the live @ref SlotState objects, which is how the
     * dispatch path reaches the per-slot @c deliverMutex and
     * @c lifecycleMutex.
     */
    [[nodiscard]] virtual std::vector<SubscriptionSlot>
        snapshotSubscriptions() const = 0;

    IBusControlBlock(const IBusControlBlock &)            = delete;
    IBusControlBlock &operator=(const IBusControlBlock &) = delete;
    IBusControlBlock(IBusControlBlock &&)                 = delete;
    IBusControlBlock &operator=(IBusControlBlock &&)      = delete;

  protected:
    IBusControlBlock() = default;
};

} // namespace vigine::messaging
