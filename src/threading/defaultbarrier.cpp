#include "defaultbarrier.h"

#include <chrono>
#include <cstdint>
#include <mutex>

namespace vigine::threading
{
namespace
{
// Clamp the constructor party count so that parties == 0 degenerates
// into a trivial one-party barrier rather than an arithmetic trap.
[[nodiscard]] std::size_t clampParties(std::size_t parties) noexcept
{
    return parties == 0 ? std::size_t{1} : parties;
}
} // namespace

DefaultBarrier::DefaultBarrier(std::size_t parties) noexcept
    : _parties{clampParties(parties)},
      _pending{clampParties(parties)},
      _generation{0}
{
}

DefaultBarrier::~DefaultBarrier() = default;

Result DefaultBarrier::arriveAndWait(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(_mutex);
    const std::uint64_t          myGeneration = _generation;
    if (_pending == 0)
    {
        // Degenerate "no parties expected" barrier (for instance after
        // arriveAndDrop drained every party). Treat the call as a no-op
        // success — the barrier has nothing to wait for.
        return Result{Result::Code::Success};
    }

    --_pending;
    if (_pending == 0)
    {
        // Last arrival — advance the generation, reset the pending
        // count, and wake every waiter that captured the previous
        // generation.
        _pending = _parties;
        ++_generation;
        _cv.notify_all();
        return Result{Result::Code::Success};
    }

    if (timeout == std::chrono::milliseconds::max())
    {
        _cv.wait(lock, [&] { return _generation != myGeneration; });
        return Result{Result::Code::Success};
    }

    if (!_cv.wait_for(lock, timeout, [&] { return _generation != myGeneration; }))
    {
        // Timed out before the phase completed. Restore the pending
        // count so the caller's earlier arrival does not count against
        // future phases.
        ++_pending;
        return Result{Result::Code::Error, "threading: barrier arrive_and_wait timeout"};
    }
    return Result{Result::Code::Success};
}

void DefaultBarrier::arriveAndDrop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    // Drop the party count for future phases.
    if (_parties > 0)
    {
        --_parties;
    }

    if (_pending == 0)
    {
        return;
    }

    --_pending;
    if (_pending == 0)
    {
        // Dropping the last outstanding arrival completes the phase.
        _pending = _parties;
        ++_generation;
        _cv.notify_all();
    }
}

std::size_t DefaultBarrier::pendingParties() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _pending;
}

} // namespace vigine::threading
