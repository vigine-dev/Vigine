#pragma once

#include <memory>

#include "vigine/eventscheduler/abstracteventscheduler.h"
#include "vigine/eventscheduler/ieventhandle.h"
#include "vigine/eventscheduler/iossignalsource.h"
#include "vigine/eventscheduler/itimersource.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::eventscheduler
{

/**
 * @brief Concrete final event-scheduler facade.
 *
 * @ref DefaultEventScheduler is Level-5 of the five-layer wrapper recipe.
 * It wires a platform @ref ITimerSource and @ref IOsSignalSource into the
 * @ref AbstractEventScheduler chain and implements @ref IEventScheduler
 * by listening for timer and OS-signal callbacks, then posting
 * @ref vigine::messaging::MessageKind::Event messages to the underlying
 * @ref vigine::messaging::AbstractMessageBus.
 *
 * Callers obtain instances exclusively through
 * @ref createEventScheduler — they never construct this type by name.
 *
 * Thread-safety: @ref schedule, @ref shutdown, and the timer/OS-signal
 * callbacks are all safe to call from any thread concurrently. Internal
 * state (the scheduled-events map) is guarded by a @c std::shared_mutex.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref schedule returns @c std::unique_ptr<IEventHandle>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultEventScheduler final
    : public AbstractEventScheduler
    , private ITimerFiredListener
    , private IOsSignalListener
{
  public:
    /**
     * @brief Constructs the scheduler.
     *
     * @p threadManager backs the internal bus.
     * @p timerSource   provides timer arm/disarm (owned by caller).
     * @p osSignalSource provides OS-signal subscribe/unsubscribe (owned by caller).
     */
    explicit DefaultEventScheduler(vigine::threading::IThreadManager &threadManager,
                                   ITimerSource                      &timerSource,
                                   IOsSignalSource                   &osSignalSource);

    ~DefaultEventScheduler() override;

    // IEventScheduler
    [[nodiscard]] std::unique_ptr<IEventHandle>
        schedule(const EventConfig                         &config,
                 vigine::messaging::AbstractMessageTarget  *target) override;

    vigine::Result shutdown() override;

    DefaultEventScheduler(const DefaultEventScheduler &)            = delete;
    DefaultEventScheduler &operator=(const DefaultEventScheduler &) = delete;
    DefaultEventScheduler(DefaultEventScheduler &&)                  = delete;
    DefaultEventScheduler &operator=(DefaultEventScheduler &&)       = delete;

  private:
    // ITimerFiredListener
    void onTimerFired(std::uint64_t timerId) override;

    // IOsSignalListener
    void onOsSignal(OsSignal signal) override;

    // Pimpl hides the scheduled-events map and atomic shutdown flag so
    // the private implementation details are not exposed in the public header.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Factory function — the sole entry point for creating an
 *        event-scheduler facade.
 *
 * Returns a @c std::unique_ptr so the caller owns the facade exclusively
 * (FF-1). The internal bus and timer/OS-signal sources are backed by the
 * supplied references; all three must outlive the returned scheduler.
 */
[[nodiscard]] std::unique_ptr<IEventScheduler>
    createEventScheduler(vigine::threading::IThreadManager &threadManager,
                         ITimerSource                      &timerSource,
                         IOsSignalSource                   &osSignalSource);

} // namespace vigine::eventscheduler
