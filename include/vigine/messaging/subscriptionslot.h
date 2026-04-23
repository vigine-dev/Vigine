#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "vigine/messaging/messagefilter.h"

namespace vigine::messaging
{
class ISubscriber;

/**
 * @brief Shared mutable per-slot state that survives value-copies of the
 *        owning @ref SubscriptionSlot.
 *
 * The dispatch hot path takes a VALUE snapshot of each
 * @ref SubscriptionSlot before the registry releases its lock, so any
 * object that has to remain the same instance across every snapshot copy
 * must live behind a @c std::shared_ptr. @ref SlotState is exactly that
 * object: it carries the per-subscriber mutexes plus the cancellation
 * flag that pair with the @ref ISubscriptionToken contract.
 *
 * Two guarantees ride on this struct:
 *
 *   - @b Per-subscriber @b serialisation. @c deliverMutex is held
 *     exclusively by the dispatch path for the entire duration of the
 *     subscriber's @c onMessage, so two dispatch threads racing the
 *     registry can never interleave @c onMessage calls on the same
 *     subscriber.
 *
 *   - @b Dtor-blocks @b contract. @c lifecycleMutex is a
 *     @c std::shared_mutex. The dispatch path acquires it in SHARED
 *     mode before calling @c onMessage and releases it after; the
 *     unsubscribe path (@ref IBusControlBlock::unregisterSubscription)
 *     acquires it in EXCLUSIVE mode, which blocks until every concurrent
 *     shared-holder has returned. Once the exclusive lock is held,
 *     @c cancelled is flipped to @c true so that snapshot copies still
 *     waiting to enter the shared region observe the flag and skip
 *     @c onMessage entirely. That is the use-after-free barrier for a
 *     subscriber whose owner drops the token and then destroys the
 *     subscriber right after the cancel returns.
 *
 * The struct is non-copyable and non-movable because the contained
 * mutexes are neither. A lifetime share through @c std::shared_ptr is
 * how snapshot copies all point at the same live object.
 */
struct SlotState
{
    /// Serialises concurrent @c onMessage calls to the same subscriber.
    std::mutex        deliverMutex;
    /// Guards the slot's active lifetime: shared for dispatch, exclusive
    /// for cancellation. See class comment above.
    std::shared_mutex lifecycleMutex;
    /// Set to @c true under the exclusive @c lifecycleMutex by the
    /// unsubscribe path. The dispatch path checks it under the shared
    /// lock and skips @c onMessage when true.
    bool              cancelled{false};

    SlotState()                             = default;
    ~SlotState()                            = default;
    SlotState(const SlotState &)            = delete;
    SlotState &operator=(const SlotState &) = delete;
    SlotState(SlotState &&)                 = delete;
    SlotState &operator=(SlotState &&)      = delete;
};

/**
 * @brief One subscription registry entry.
 *
 * Holds the raw, non-owning subscriber pointer, the filter the caller
 * supplied to @ref IMessageBus::subscribe, a bus-assigned serial id that
 * addresses the slot inside the registry, and a shared @ref SlotState
 * so every snapshot copy of the slot refers to the same mutexes and
 * cancellation flag.
 *
 * @c slotState is @c shared_ptr-owned on purpose: the dispatch path
 * takes a VALUE snapshot of each slot before the registry unlocks, and
 * neither @c std::mutex nor @c std::shared_mutex is copyable. Sharing
 * through @c shared_ptr keeps every copy pointing at the same live
 * object, so the serialisation and dtor-blocks guarantees hold even
 * through the snapshot path.
 *
 * The struct is a plain aggregate with value semantics: copying a slot
 * copies the pointer, the filter, the serial, the active flag, and
 * bumps the @c SlotState ref-count.
 */
struct SubscriptionSlot
{
    ISubscriber               *subscriber{nullptr};
    MessageFilter              filter{};
    std::uint64_t              serial{0};
    bool                       active{true};
    std::shared_ptr<SlotState> slotState;
};

} // namespace vigine::messaging
