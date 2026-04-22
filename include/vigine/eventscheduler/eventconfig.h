#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "vigine/eventscheduler/ossignal.h"
#include "vigine/payload/payloadtypeid.h"

namespace vigine::eventscheduler
{

/**
 * @brief POD configuration for a scheduled event.
 *
 * Callers fill this struct and pass it to
 * @ref IEventScheduler::schedule. The fields map to the three input
 * examples:
 *
 *   - Timer + count:  @c period > 0, @c count > 0, @c useOsSignal == false.
 *   - One-shot delay: @c delay > 0, @c period == 0, @c count == 1,
 *                     @c useOsSignal == false.
 *   - OS signal:      @c useOsSignal == true, @c osSignal set accordingly;
 *                     all time fields are ignored.
 *
 * Field semantics:
 *   - @c delay  -- first fire after this interval; 0 = fire immediately on arm.
 *   - @c period -- repeat interval; 0 = one-shot (fire once and disarm).
 *   - @c count  -- maximum fire count; 0 = unlimited repetition
 *                  (only meaningful when period > 0).
 *   - @c useOsSignal -- when @c true, arm the OS signal watcher instead of a timer.
 *   - @c osSignal -- OS signal to watch; only used when @c useOsSignal is @c true.
 *   - @c rescheduleWhileRunning -- when @c false (default), skip a fire if
 *                                   the previous delivery is still in-flight.
 *   - @c firedPayloadTypeId     -- payload type id carried by the dispatched
 *                                   IMessage; zero-value = scheduler supplies a
 *                                   built-in default.
 *
 * INV-1: no template parameters.
 * INV-11: no graph types in this header.
 */
struct EventConfig
{
    std::chrono::milliseconds         delay{0};
    std::chrono::milliseconds         period{0};
    std::size_t                       count{0};
    bool                              useOsSignal{false};
    OsSignal                          osSignal{OsSignal::Terminate};
    bool                              rescheduleWhileRunning{false};
    vigine::payload::PayloadTypeId    firedPayloadTypeId{};

    /** @brief Returns true when this config arms an OS-signal watcher. */
    [[nodiscard]] bool isOsSignalTrigger() const noexcept
    {
        return useOsSignal;
    }
};

} // namespace vigine::eventscheduler
