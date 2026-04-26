#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "vigine/api/eventscheduler/itimersource.h"

namespace vigine::eventscheduler
{

struct TimerEntry;

/**
 * @brief Cross-platform timer source using a dedicated worker thread.
 *
 * Uses @c std::chrono::steady_clock + @c std::condition_variable::wait_until.
 * Resolution: ~1 ms on POSIX; ~10-15 ms on Windows without timeBeginPeriod.
 *
 * Private to @c src/eventscheduler/. Callers only see @ref ITimerSource.
 */
class DefaultTimerSource final : public ITimerSource
{
  public:
    DefaultTimerSource();
    ~DefaultTimerSource() override;

    [[nodiscard]] std::uint64_t armOneShot(std::chrono::milliseconds delay,
                                           ITimerFiredListener       *listener) override;

    [[nodiscard]] std::uint64_t armPeriodic(std::chrono::milliseconds period,
                                             std::size_t               count,
                                             ITimerFiredListener       *listener) override;

    void disarm(std::uint64_t timerId) override;

  private:
    void run();

    std::atomic<std::uint64_t>  _nextId;
    std::mutex                  _mutex;
    std::condition_variable     _cv;
    std::vector<TimerEntry>     _timers;
    bool                        _stop{false};
    std::thread                 _thread;
};

} // namespace vigine::eventscheduler
