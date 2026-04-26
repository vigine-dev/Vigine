#include "vigine/core/threading/abstractthreadmanager.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace vigine::core::threading
{
namespace
{
// Resolve a requested size against hardware_concurrency with a sensible
// floor. A requested size of 0 asks the factory to derive from
// hardware_concurrency; if that reports 0 (rare), fall back to 1.
[[nodiscard]] std::size_t resolveSize(std::size_t requested) noexcept
{
    if (requested != 0)
    {
        return requested;
    }
    const unsigned hc = std::thread::hardware_concurrency();
    if (hc == 0)
    {
        return 1;
    }
    return static_cast<std::size_t>(hc);
}
} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

AbstractThreadManager::AbstractThreadManager(ThreadManagerConfig config) noexcept
    : _config{config}
{
    _config.poolSize            = resolveSize(_config.poolSize);
    _config.maxDedicatedThreads = resolveSize(_config.maxDedicatedThreads);
    // maxNamedThreads keeps its user-provided value; a zero here would
    // reject every registration, which is a legitimate stance for a
    // test harness that does not use named threads.
}

AbstractThreadManager::~AbstractThreadManager() = default;

// ---------------------------------------------------------------------------
// Registry: named threads.
// ---------------------------------------------------------------------------

NamedThreadId AbstractThreadManager::registerNamedThread(std::string_view name)
{
    if (_shutDown.load(std::memory_order_acquire))
    {
        return NamedThreadId{};
    }

    std::lock_guard<std::mutex> lock(_registryMutex);
    if (_namedLiveCount >= _config.maxNamedThreads)
    {
        return NamedThreadId{};
    }

    // Reuse a dead slot if one exists; otherwise append a fresh slot.
    std::size_t idx = _namedSlots.size();
    for (std::size_t i = 0; i < _namedSlots.size(); ++i)
    {
        if (!_namedSlots[i].live)
        {
            idx = i;
            break;
        }
    }

    const std::uint32_t generation =
        _nextNamedGeneration.fetch_add(1, std::memory_order_relaxed);
    if (idx == _namedSlots.size())
    {
        NamedSlot slot;
        slot.name       = std::string{name};
        slot.generation = generation;
        slot.live       = true;
        _namedSlots.push_back(std::move(slot));
    }
    else
    {
        _namedSlots[idx].name       = std::string{name};
        _namedSlots[idx].generation = generation;
        _namedSlots[idx].live       = true;
    }
    ++_namedLiveCount;
    return NamedThreadId{static_cast<std::uint32_t>(idx), generation};
}

void AbstractThreadManager::unregisterNamedThread(NamedThreadId id)
{
    if (!id.valid())
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_registryMutex);
    if (id.index >= _namedSlots.size())
    {
        return;
    }
    NamedSlot &slot = _namedSlots[id.index];
    if (!slot.live || slot.generation != id.generation)
    {
        return;
    }
    slot.live = false;
    slot.name.clear();
    // generation stays so that a stale id with the old generation still
    // fails resolveNamedSlot even after the slot is reused.
    if (_namedLiveCount > 0)
    {
        --_namedLiveCount;
    }
}

std::size_t AbstractThreadManager::resolveNamedSlot(NamedThreadId id) const noexcept
{
    if (!id.valid())
    {
        return std::numeric_limits<std::size_t>::max();
    }
    std::lock_guard<std::mutex> lock(_registryMutex);
    if (id.index >= _namedSlots.size())
    {
        return std::numeric_limits<std::size_t>::max();
    }
    const NamedSlot &slot = _namedSlots[id.index];
    if (!slot.live || slot.generation != id.generation)
    {
        return std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::size_t>(id.index);
}

std::string AbstractThreadManager::namedThreadName(NamedThreadId id) const
{
    if (!id.valid())
    {
        return {};
    }
    std::lock_guard<std::mutex> lock(_registryMutex);
    if (id.index >= _namedSlots.size())
    {
        return {};
    }
    const NamedSlot &slot = _namedSlots[id.index];
    if (!slot.live || slot.generation != id.generation)
    {
        return {};
    }
    return slot.name;
}

// ---------------------------------------------------------------------------
// Observability.
// ---------------------------------------------------------------------------

std::size_t AbstractThreadManager::poolSize() const noexcept
{
    return _config.poolSize;
}

std::size_t AbstractThreadManager::dedicatedThreadCount() const noexcept
{
    return _dedicatedCount.load(std::memory_order_acquire);
}

std::size_t AbstractThreadManager::namedThreadCount() const noexcept
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    return _namedLiveCount;
}

// ---------------------------------------------------------------------------
// Protected helpers exposed to derived schedulers.
// ---------------------------------------------------------------------------

const ThreadManagerConfig &AbstractThreadManager::config() const noexcept
{
    return _config;
}

bool AbstractThreadManager::isShutDown() const noexcept
{
    return _shutDown.load(std::memory_order_acquire);
}

void AbstractThreadManager::markShutDown() noexcept
{
    _shutDown.store(true, std::memory_order_release);
}

bool AbstractThreadManager::acquireDedicatedSlot() noexcept
{
    // CAS loop honouring the `maxDedicatedThreads` cap. Two concurrent
    // callers racing at the cap boundary are serialised through the
    // atomic compare-exchange: only one can bump the counter from
    // `cap - 1` to `cap`; the other observes the updated value and
    // returns false.
    const std::size_t cap = _config.maxDedicatedThreads;
    std::size_t current   = _dedicatedCount.load(std::memory_order_acquire);
    while (true)
    {
        if (current >= cap)
        {
            return false;
        }
        if (_dedicatedCount.compare_exchange_weak(
                current, current + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            return true;
        }
    }
}

void AbstractThreadManager::releaseDedicatedSlot() noexcept
{
    // Saturate at zero. A concurrent pair of increments/decrements cannot
    // drive the counter below zero under normal use, but a defensive
    // clamp costs nothing and protects against bookkeeping slips.
    std::size_t prev = _dedicatedCount.load(std::memory_order_acquire);
    while (prev > 0)
    {
        if (_dedicatedCount.compare_exchange_weak(
                prev, prev - 1, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return;
        }
    }
}

} // namespace vigine::core::threading
