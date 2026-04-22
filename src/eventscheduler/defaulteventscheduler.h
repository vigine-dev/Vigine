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
 * `DefaultEventScheduler` is the concrete implementation behind the
 * `createEventScheduler` public factory (declared in
 * `include/vigine/eventscheduler/factory.h`). Callers never name this
 * type directly — it lives under `src/eventscheduler/` precisely
 * because nothing outside the implementation needs it. The factory
 * returns a `std::unique_ptr<IEventScheduler>`, so consumers only
 * see the interface.
 *
 * The class wires a platform @ref ITimerSource and @ref IOsSignalSource
 * into the @ref AbstractEventScheduler chain and implements
 * @ref IEventScheduler by listening for timer and OS-signal callbacks,
 * then posting @ref vigine::messaging::MessageKind::Event messages to
 * the underlying bus.
 *
 * Thread-safety: `schedule`, `shutdown`, and the timer / OS-signal
 * callbacks are all safe to call from any thread concurrently. The
 * scheduled-events map is guarded by a `std::shared_mutex`.
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
     * @p timerSource   provides timer arm / disarm (owned by caller).
     * @p osSignalSource provides OS-signal subscribe / unsubscribe
     *                   (owned by caller).
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
    // the private implementation details stay private.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::eventscheduler
