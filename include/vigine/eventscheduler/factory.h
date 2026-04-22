#pragma once

#include <memory>

#include "vigine/eventscheduler/ieventscheduler.h"
#include "vigine/eventscheduler/iossignalsource.h"
#include "vigine/eventscheduler/itimersource.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::eventscheduler
{
/**
 * @brief Factory function — the sole public entry point for creating
 *        an event-scheduler facade.
 *
 * Returns a `std::unique_ptr` so the caller owns the facade
 * exclusively. The concrete `DefaultEventScheduler` type is an
 * internal implementation detail under `src/eventscheduler/`;
 * callers only need this header and the @ref IEventScheduler
 * interface.
 *
 * All three references must outlive the returned scheduler:
 *
 * - `@p threadManager` backs the scheduler's internal messaging.
 * - `@p timerSource`   provides timer arm / disarm.
 * - `@p osSignalSource` provides OS-signal subscribe /
 *   unsubscribe.
 */
[[nodiscard]] std::unique_ptr<IEventScheduler>
    createEventScheduler(vigine::threading::IThreadManager &threadManager,
                         ITimerSource                      &timerSource,
                         IOsSignalSource                   &osSignalSource);

} // namespace vigine::eventscheduler
