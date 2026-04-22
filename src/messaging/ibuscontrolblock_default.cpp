#include "messaging/ibuscontrolblock_default.h"

#include <mutex>

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

ConnectionId
DefaultBusControlBlock::allocateSlot(AbstractMessageTarget *target)
{
    if (target == nullptr)
    {
        return ConnectionId{};
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        return ConnectionId{};
    }

    std::unique_lock<std::shared_mutex> lock{_registryMutex};

    // Re-check under the lock: another thread may have raced ahead and
    // called markDead between the fast-path read and the lock.
    if (!_alive.load(std::memory_order_acquire))
    {
        return ConnectionId{};
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

    _registry.emplace(index, Slot{target, generation});
    return ConnectionId{index, generation};
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

    std::unique_lock<std::shared_mutex> lock{_registryMutex};
    auto it = _registry.find(id.index);
    if (it == _registry.end())
    {
        return;
    }
    if (it->second.generation != id.generation)
    {
        // The slot was already recycled under a newer generation; the
        // caller's id is stale and must not touch the live slot.
        return;
    }
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
    std::shared_lock<std::shared_mutex> lock{_registryMutex};
    auto it = _registry.find(id.index);
    if (it == _registry.end())
    {
        return {};
    }
    if (it->second.generation != id.generation)
    {
        return {};
    }
    // Hand the lock over to the caller alongside the pointer. The
    // RAII guard releases the lock when the caller's scope ends,
    // so a concurrent unregisterTarget cannot retire the slot while
    // the caller dereferences `target`.
    return LookupGuard{it->second.target, std::move(lock)};
}

} // namespace vigine::messaging
