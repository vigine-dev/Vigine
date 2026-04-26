#include "itimersource_default.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace vigine::eventscheduler
{

// -----------------------------------------------------------------
// TimerEntry — one registered timer.
// -----------------------------------------------------------------

struct TimerEntry
{
    std::uint64_t                         id{0};
    std::chrono::steady_clock::time_point nextFire{};
    std::chrono::milliseconds             period{0};
    std::size_t                           count{0};
    std::size_t                           fired{0};
    ITimerFiredListener                  *listener{nullptr};
    bool                                  active{true};
};

// -----------------------------------------------------------------
// DefaultTimerSource — cross-platform implementation using
// std::condition_variable::wait_until + a dedicated worker thread.
// Resolution: ~1 ms on Linux/macOS, ~10-15 ms on Windows by default
// (global timeBeginPeriod not applied to avoid side-effects).
// -----------------------------------------------------------------

DefaultTimerSource::DefaultTimerSource()
    : _nextId(1)
    , _stop(false)
{
    _thread = std::thread([this] { run(); });
}

DefaultTimerSource::~DefaultTimerSource()
{
    {
        std::unique_lock lock(_mutex);
        _stop = true;
    }
    _cv.notify_all();
    if (_thread.joinable())
    {
        _thread.join();
    }
}

std::uint64_t DefaultTimerSource::armOneShot(
    std::chrono::milliseconds delay,
    ITimerFiredListener       *listener)
{
    std::uint64_t id = _nextId.fetch_add(1, std::memory_order_relaxed);
    auto now = std::chrono::steady_clock::now();
    {
        std::unique_lock lock(_mutex);
        TimerEntry entry;
        entry.id       = id;
        entry.nextFire = now + delay;
        entry.period   = std::chrono::milliseconds{0};
        entry.count    = 1;
        entry.fired    = 0;
        entry.listener = listener;
        entry.active   = true;
        _timers.push_back(std::move(entry));
    }
    _cv.notify_one();
    return id;
}

std::uint64_t DefaultTimerSource::armPeriodic(
    std::chrono::milliseconds period,
    std::size_t               count,
    ITimerFiredListener       *listener)
{
    std::uint64_t id = _nextId.fetch_add(1, std::memory_order_relaxed);
    auto now = std::chrono::steady_clock::now();
    {
        std::unique_lock lock(_mutex);
        TimerEntry entry;
        entry.id       = id;
        entry.nextFire = now + period;
        entry.period   = period;
        entry.count    = count;
        entry.fired    = 0;
        entry.listener = listener;
        entry.active   = true;
        _timers.push_back(std::move(entry));
    }
    _cv.notify_one();
    return id;
}

void DefaultTimerSource::disarm(std::uint64_t timerId)
{
    std::unique_lock lock(_mutex);
    for (auto &entry : _timers)
    {
        if (entry.id == timerId)
        {
            entry.active = false;
        }
    }
}

void DefaultTimerSource::run()
{
    while (true)
    {
        std::chrono::steady_clock::time_point nextWakeup;
        bool hasTimer = false;

        std::vector<std::pair<ITimerFiredListener *, std::uint64_t>> toFire;

        {
            std::unique_lock lock(_mutex);
            auto now = std::chrono::steady_clock::now();

            // Remove inactive entries.
            _timers.erase(
                std::remove_if(_timers.begin(), _timers.end(),
                               [](const TimerEntry &e) { return !e.active; }),
                _timers.end());

            // Collect fired timers.
            for (auto &entry : _timers)
            {
                if (!entry.active)
                {
                    continue;
                }
                if (entry.nextFire <= now)
                {
                    toFire.emplace_back(entry.listener, entry.id);
                    entry.fired++;

                    if (entry.period.count() > 0
                        && (entry.count == 0 || entry.fired < entry.count))
                    {
                        // Reschedule.
                        entry.nextFire = now + entry.period;
                    }
                    else
                    {
                        entry.active = false;
                    }
                }
            }

            // Find next wake-up.
            for (const auto &entry : _timers)
            {
                if (!entry.active)
                {
                    continue;
                }
                if (!hasTimer || entry.nextFire < nextWakeup)
                {
                    nextWakeup = entry.nextFire;
                    hasTimer   = true;
                }
            }

            if (_stop && _timers.empty())
            {
                break;
            }

            if (!hasTimer)
            {
                _cv.wait(lock, [this] {
                    return _stop || !_timers.empty();
                });
                if (_stop)
                {
                    break;
                }
                continue;
            }

            _cv.wait_until(lock, nextWakeup, [this] { return _stop; });

            if (_stop)
            {
                break;
            }
        }

        // Fire callbacks outside the lock.
        for (auto &[listener, id] : toFire)
        {
            if (listener)
            {
                listener->onTimerFired(id);
            }
        }
    }
}

} // namespace vigine::eventscheduler
