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

    const std::uint32_t index      = _nextIndex++;
    const std::uint32_t generation = _nextGeneration++;
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
    _registry.erase(it);
}

AbstractMessageTarget *
DefaultBusControlBlock::lookup(ConnectionId id) const noexcept
{
    if (!id.valid())
    {
        return nullptr;
    }
    if (!_alive.load(std::memory_order_acquire))
    {
        return nullptr;
    }
    std::shared_lock<std::shared_mutex> lock{_registryMutex};
    auto it = _registry.find(id.index);
    if (it == _registry.end())
    {
        return nullptr;
    }
    if (it->second.generation != id.generation)
    {
        return nullptr;
    }
    return it->second.target;
}

} // namespace vigine::messaging
