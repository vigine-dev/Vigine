#include "messaging/ibuscontrolblock_default.h"

#include <mutex>
#include <utility>

#include "vigine/messaging/isubscriber.h"

namespace vigine::messaging
{

DefaultBusControlBlock::DefaultBusControlBlock() noexcept = default;

DefaultBusControlBlock::~DefaultBusControlBlock()
{
    // markDead is idempotent; running it from the destructor makes sure
    // that any token that outlives this block but observes the weak_ptr
    // lock() before the shared count drops to zero still sees the bus
    // as dead and skips its unregister call.
    markDead();
}

bool DefaultBusControlBlock::isAlive() const noexcept
{
    return _alive.load(std::memory_order_acquire);
}

void DefaultBusControlBlock::markDead() noexcept
{
    _alive.store(false, std::memory_order_release);
}

SlotAllocation
DefaultBusControlBlock::allocateSlot(AbstractMessageTarget *target)
{
    if (target == nullptr)
    {
        return SlotAllocation{};
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        return SlotAllocation{};
    }

    // Build the SlotState before taking the registry lock: SlotState's
    // mutexes are constructed under no lock, and a `bad_alloc` here
    // bubbles back to the caller without leaving any registry mutation
    // behind. The shared_ptr is moved into both the registry slot and
    // the SlotAllocation, so the registry, the dispatch path, and the
    // ConnectionToken all share one live SlotState — exactly the same
    // shape as the subscriber side.
    auto slotState = std::make_shared<SlotState>();

    std::unique_lock<std::shared_mutex> lock{_registryMutex};

    // Re-check under the lock: another thread may have raced ahead and
    // called markDead between the fast-path read and the lock.
    if (!_alive.load(std::memory_order_acquire))
    {
        return SlotAllocation{};
    }

    // Prefer a recycled index so the registry does not grow
    // monotonically under subscribe / unsubscribe churn.
    std::uint32_t index;
    std::uint32_t generation;
    if (!_freeIndices.empty())
    {
        index = _freeIndices.back();
        _freeIndices.pop_back();
        auto genIt = _nextGenerationByIndex.find(index);
        generation = (genIt != _nextGenerationByIndex.end()) ? genIt->second
                                                             : _nextGeneration++;
        // Wrap-around guard: generation 0 is the invalid sentinel.
        if (generation == 0u)
        {
            generation = 1u;
        }
    }
    else
    {
        index      = _nextIndex++;
        generation = _nextGeneration++;
        if (generation == 0u)
        {
            generation = _nextGeneration++;
        }
    }

    _registry.emplace(index, Slot{target, generation, slotState});
    return SlotAllocation{ConnectionId{index, generation}, std::move(slotState)};
}

void DefaultBusControlBlock::unregisterTarget(ConnectionId id) noexcept
{
    if (!id.valid())
    {
        return;
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        // Fast-path after markDead: drop the lock acquisition entirely
        // and let the destructor reclaim the registry en masse.
        return;
    }

    // Capture the SlotState shared_ptr BEFORE erasing the registry
    // entry so the lifecycle mutex survives the map removal. Any
    // dispatch already past the registry lookup owns its own
    // shared_ptr to the same SlotState; after this capture AND the
    // erase, the lifecycle mutex below serialises against every such
    // in-flight onMessage before we return — which is the dtor-blocks
    // guarantee that the ConnectionToken contract advertises. Mirror
    // of `unregisterSubscription`.
    std::shared_ptr<SlotState> state;
    {
        std::unique_lock<std::shared_mutex> lock{_registryMutex};
        auto it = _registry.find(id.index);
        if (it == _registry.end())
        {
            return;
        }
        if (it->second.generation != id.generation)
        {
            // The slot was already recycled under a newer generation;
            // the caller's id is stale and must not touch the live
            // slot.
            return;
        }
        state = it->second.slotState;
        // Remember the next generation for this index, erase the slot,
        // push the index onto the free-list. `allocateSlot` picks it
        // up on its next call. Previous behaviour left the slot in
        // place as a null-target tombstone; over long sessions the
        // registry grew without bound.
        std::uint32_t nextGen = it->second.generation + 1u;
        if (nextGen == 0u)
        {
            nextGen = 1u;
        }
        _nextGenerationByIndex[id.index] = nextGen;
        _registry.erase(it);
        _freeIndices.push_back(id.index);
    }

    if (state)
    {
        // Acquire lifecycleMutex EXCLUSIVELY. Every concurrent
        // dispatch that reached this slot through `lookup` holds it
        // SHARED for the duration of onMessage, so the exclusive
        // acquisition blocks until all of them have returned. Once we
        // hold the lock we flip `cancelled` with a release store so
        // that any later lookup racing the registry erase — including
        // the lock-free `ConnectionToken::active` reader — observes
        // the flag and returns an empty guard / `false`. Matching the
        // subscriber-side flow.
        std::unique_lock<std::shared_mutex> ex{state->lifecycleMutex};
        state->cancelled.store(true, std::memory_order_release);
    }
}

std::uint64_t DefaultBusControlBlock::registerSubscription(
    ISubscriber               *subscriber,
    MessageFilter              filter,
    std::shared_ptr<SlotState> slotState)
{
    if (subscriber == nullptr)
    {
        return 0;
    }
    if (!slotState)
    {
        return 0;
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        return 0;
    }

    std::unique_lock<std::shared_mutex> lock{_subscriptionRegistryMutex};

    // Re-check under the lock: a concurrent markDead may have raced
    // ahead between the fast-path read and the lock. Without the re-
    // check a bus caught mid-shutdown could still hand out a serial
    // that the bus's subscribe() wraps in a token, with no one left
    // to drain it.
    if (!_alive.load(std::memory_order_acquire))
    {
        return 0;
    }

    std::uint64_t serial = _nextSerial++;
    // Wrap-around guard: serial 0 is the inert sentinel that the
    // token path uses to short-circuit active() / cancel(). Skip it
    // on overflow so the registry never emits it for a live slot.
    if (serial == 0)
    {
        serial      = _nextSerial++;
    }

    SubscriptionSlot slot{};
    slot.subscriber = subscriber;
    slot.filter     = std::move(filter);
    slot.serial     = serial;
    slot.active     = true;
    slot.slotState  = std::move(slotState);

    _subscriptions.emplace(serial, std::move(slot));
    return serial;
}

void DefaultBusControlBlock::unregisterSubscription(std::uint64_t serial) noexcept
{
    if (serial == 0)
    {
        return;
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        // Fast-path after markDead: the control block's destructor
        // reclaims the whole subscription map en masse. Running the
        // per-slot drain here against a dead block is pointless —
        // the bus is gone, no dispatcher can still be looking at
        // these slots.
        return;
    }

    // Capture the SlotState shared_ptr BEFORE erasing the registry
    // entry so the lifecycle mutex survives the map removal. A
    // snapshot copy still in flight through the dispatch path owns
    // its own shared_ptr to the same SlotState; after this capture
    // AND the erase, the lifecycle mutex below serialises against
    // every such in-flight deliver() before we return — which is the
    // dtor-blocks guarantee that the token contract advertises.
    std::shared_ptr<SlotState> state;
    {
        std::unique_lock<std::shared_mutex> lock{_subscriptionRegistryMutex};
        auto it = _subscriptions.find(serial);
        if (it == _subscriptions.end())
        {
            return;
        }
        state = it->second.slotState;
        _subscriptions.erase(it);
    }

    if (state)
    {
        // Acquire lifecycleMutex EXCLUSIVELY. Every concurrent
        // deliver() holds it SHARED for the duration of onMessage, so
        // the exclusive acquisition blocks until all of them have
        // returned. Once we hold the lock we flip `cancelled` with a
        // release store so that any snapshot copy still waiting to
        // enter the shared region will observe the flag and skip
        // onMessage without running the subscriber.
        std::unique_lock<std::shared_mutex> ex{state->lifecycleMutex};
        state->cancelled.store(true, std::memory_order_release);
    }
}

std::vector<SubscriptionSlot> DefaultBusControlBlock::snapshotSubscriptions() const
{
    std::vector<SubscriptionSlot>       snapshot;
    std::shared_lock<std::shared_mutex> lock{_subscriptionRegistryMutex};
    snapshot.reserve(_subscriptions.size());
    for (const auto &entry : _subscriptions)
    {
        if (entry.second.active)
        {
            snapshot.push_back(entry.second);
        }
    }
    return snapshot;
}

DefaultBusControlBlock::LookupGuard
DefaultBusControlBlock::lookup(ConnectionId id) const noexcept
{
    if (!id.valid())
    {
        return {};
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        return {};
    }
    std::shared_lock<std::shared_mutex> registryLock{_registryMutex};
    auto it = _registry.find(id.index);
    if (it == _registry.end())
    {
        return {};
    }
    if (it->second.generation != id.generation)
    {
        return {};
    }

    AbstractMessageTarget *const     target    = it->second.target;
    const std::shared_ptr<SlotState> slotState = it->second.slotState;

    // Acquire the slot's lifecycleMutex in SHARED mode AFTER we have
    // identified the live slot. This pairs with the exclusive lock
    // taken by `unregisterTarget`: while we hold the shared lock, the
    // cancel barrier inside `~ConnectionToken` cannot return, so the
    // target pointer remains live for the duration of the caller's
    // onMessage call. Mirror of the subscriber dispatch path in
    // `AbstractMessageBus::deliver`.
    std::shared_lock<std::shared_mutex> lifecycleLock;
    if (slotState)
    {
        lifecycleLock = std::shared_lock<std::shared_mutex>{slotState->lifecycleMutex};
        if (slotState->cancelled.load(std::memory_order_acquire))
        {
            // The slot was cancelled between the registry probe and
            // the lifecycle acquisition; an empty guard skips
            // onMessage and prevents use-after-free on a target whose
            // owner is mid-destruction.
            return {};
        }
    }

    // Hand both locks over to the caller alongside the pointer. The
    // RAII guard releases them when the caller's scope ends, so neither
    // a concurrent unregisterTarget nor a concurrent token destructor
    // can retire the slot while the caller dereferences `target`.
    return LookupGuard{target, std::move(registryLock), std::move(lifecycleLock)};
}

} // namespace vigine::messaging
