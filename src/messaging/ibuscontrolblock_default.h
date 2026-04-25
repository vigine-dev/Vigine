#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/ibuscontrolblock.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/subscriptionslot.h"

namespace vigine::messaging
{
class AbstractMessageTarget;
class ISubscriber;

/**
 * @brief Reference implementation of @ref IBusControlBlock used by the
 *        default message bus.
 *
 * The class owns the atomic alive flag, the generational slot
 * generator, and the target registry. Mutating entry points take an
 * exclusive lock on an internal @c std::shared_mutex; the alive check
 * is wait-free through a @c std::atomic<bool> loaded with acquire
 * ordering. The registry keeps raw, non-owning pointers to
 * @ref AbstractMessageTarget objects; lifetime safety is provided by
 * @ref ConnectionToken running the corresponding @ref unregisterTarget
 * before the target is destroyed.
 *
 * The class is sized for a single bus instance. It lives under @c src
 * because it is a concrete implementation detail; the public surface
 * for callers is @ref IBusControlBlock.
 */
class DefaultBusControlBlock final : public IBusControlBlock
{
  public:
    DefaultBusControlBlock() noexcept;
    ~DefaultBusControlBlock() override;

    [[nodiscard]] bool isAlive() const noexcept override;
    void               markDead() noexcept override;

    [[nodiscard]] SlotAllocation
        allocateSlot(AbstractMessageTarget *target) override;
    void unregisterTarget(ConnectionId id) noexcept override;

    [[nodiscard]] std::uint64_t
        registerSubscription(ISubscriber               *subscriber,
                             MessageFilter              filter,
                             std::shared_ptr<SlotState> slotState) override;
    void unregisterSubscription(std::uint64_t serial) noexcept override;
    [[nodiscard]] std::vector<SubscriptionSlot>
        snapshotSubscriptions() const override;

    /**
     * @brief RAII bundle holding the registry's shared lock plus the
     *        slot's @c lifecycleMutex shared lock alongside a target
     *        pointer.
     *
     * Returned by @ref lookup so callers can dereference `target`
     * without racing either a concurrent `unregisterTarget` or a
     * `~ConnectionToken` running the cancel barrier. Both shared locks
     * release when the guard goes out of scope. Move-only (the
     * shared_lock members are move-only by construction); copies would
     * accidentally extend the locked region indefinitely.
     *
     * Usage:
     *   if (auto guard = control.lookup(id); guard.target != nullptr)
     *   {
     *       guard.target->onMessage(msg);
     *   }
     *
     * When the lookup fails (invalid id, stale generation, bus dead,
     * or the slot already cancelled), `guard.target` is `nullptr` and
     * neither shared lock is held. Callers only need to null-check
     * `target`.
     *
     * Lock-order with the cancel path is registry(shared) ->
     * lifecycleMutex(shared); `unregisterTarget` takes
     * lifecycleMutex(exclusive) AFTER releasing the registry's
     * exclusive lock and so cannot race with this nesting.
     */
    struct LookupGuard
    {
        AbstractMessageTarget               *target{nullptr};
        std::shared_lock<std::shared_mutex>  registryLock{};
        std::shared_lock<std::shared_mutex>  lifecycleLock{};
    };

    /**
     * @brief Returns an RAII guard around the live target pointer
     *        behind @p id, or an empty guard when the slot is stale,
     *        recycled, the bus is dead, or the slot has already been
     *        cancelled.
     *
     * The returned guard owns a shared_lock on the registry AND a
     * shared_lock on the slot's @c lifecycleMutex, so dereferencing
     * `guard.target` is safe against a concurrent `unregisterTarget`
     * AND against a concurrent `~ConnectionToken` driving the cancel
     * barrier. Releasing the guard releases both locks.
     */
    [[nodiscard]] LookupGuard lookup(ConnectionId id) const noexcept;

  private:
    struct Slot
    {
        AbstractMessageTarget     *target{nullptr};
        std::uint32_t              generation{0};
        std::shared_ptr<SlotState> slotState{};
    };

    mutable std::shared_mutex                     _registryMutex;
    std::unordered_map<std::uint32_t, Slot>       _registry;
    std::uint32_t                                 _nextIndex{1};
    std::uint32_t                                 _nextGeneration{1};
    // Free-list of indices vacated by `unregisterTarget`.
    // `allocateSlot` pops a recycled index before extending
    // `_nextIndex`, so the registry does not grow monotonically
    // under subscribe / unsubscribe churn. A per-index next-
    // generation tracker guarantees a recycled slot always gets a
    // generation strictly higher than any id previously issued for
    // that index — stale `ConnectionId`s that remembered the old
    // generation still fail their generational equality check.
    std::vector<std::uint32_t>                       _freeIndices;
    std::unordered_map<std::uint32_t, std::uint32_t> _nextGenerationByIndex;
    std::atomic<bool>                             _alive{true};

    // Subscription registry. Kept on the control block (not on the
    // bus) so that `SubscriptionToken` can drop its slot by locking a
    // `weak_ptr<IBusControlBlock>` exactly the way `ConnectionToken`
    // drops target slots. A bus destroyed before its tokens observes
    // the weak_ptr.lock() returning null (or isAlive() returning
    // false when the block is still reachable through another shared
    // owner) and the token becomes a no-op — no more reaching into a
    // freed bus pointer.
    mutable std::shared_mutex                           _subscriptionRegistryMutex;
    std::unordered_map<std::uint64_t, SubscriptionSlot> _subscriptions;
    std::uint64_t                                       _nextSerial{1};
};

} // namespace vigine::messaging
