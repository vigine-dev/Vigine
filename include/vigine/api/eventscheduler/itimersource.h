#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace vigine::eventscheduler
{

/**
 * @brief Listener notified when a timer managed by @ref ITimerSource fires.
 *
 * INV-10: @c I prefix for a pure-virtual interface with no state.
 */
class ITimerFiredListener
{
  public:
    virtual ~ITimerFiredListener() = default;

    /**
     * @brief Called by the timer source when the timer identified by
     *        @p timerId fires.
     *
     * Invoked from the timer thread; implementations must be
     * thread-safe. Must not block.
     */
    virtual void onTimerFired(std::uint64_t timerId) = 0;

  protected:
    ITimerFiredListener() = default;
};

/**
 * @brief Pure-virtual cross-platform timer abstraction.
 *
 * @ref ITimerSource wraps an OS timing mechanism and delivers
 * @ref ITimerFiredListener::onTimerFired callbacks on a dedicated worker
 * thread. The scheduler uses this interface so the concrete timer
 * implementation is swappable (mocked in tests, accelerated in CI).
 *
 * Ownership: the caller retains the @ref ITimerFiredListener pointer and
 * is responsible for its lifetime. The timer source never owns listeners.
 *
 * INV-1: no template parameters.
 * INV-10: @c I prefix for a pure-virtual interface with no state.
 * INV-11: no graph types in this header.
 */
class ITimerSource
{
  public:
    virtual ~ITimerSource() = default;

    /**
     * @brief Arms a one-shot timer that fires after @p delay.
     *
     * Returns a timer id that can be passed to @ref disarm. The id is
     * unique within this source instance; reuse after @ref disarm or
     * after the fire is implementation-defined.
     */
    [[nodiscard]] virtual std::uint64_t
        armOneShot(std::chrono::milliseconds delay,
                   ITimerFiredListener      *listener) = 0;

    /**
     * @brief Arms a periodic timer that fires every @p period.
     *
     * @p count controls the maximum fire count; 0 means unlimited.
     * Returns a timer id that can be passed to @ref disarm.
     */
    [[nodiscard]] virtual std::uint64_t
        armPeriodic(std::chrono::milliseconds period,
                    std::size_t               count,
                    ITimerFiredListener       *listener) = 0;

    /**
     * @brief Disarms the timer identified by @p timerId.
     *
     * Disarming an already-expired or invalid id is a no-op. An
     * in-flight callback may still complete after @ref disarm returns;
     * callers should not rely on strict before/after ordering.
     */
    virtual void disarm(std::uint64_t timerId) = 0;

    ITimerSource(const ITimerSource &)            = delete;
    ITimerSource &operator=(const ITimerSource &) = delete;
    ITimerSource(ITimerSource &&)                 = delete;
    ITimerSource &operator=(ITimerSource &&)      = delete;

  protected:
    ITimerSource() = default;
};

} // namespace vigine::eventscheduler
