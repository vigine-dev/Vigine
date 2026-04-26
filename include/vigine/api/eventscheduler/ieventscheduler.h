#pragma once

#include <memory>

#include "vigine/api/eventscheduler/eventconfig.h"
#include "vigine/api/eventscheduler/ieventhandle.h"
#include "vigine/result.h"

namespace vigine::messaging
{
class AbstractMessageTarget;
} // namespace vigine::messaging

namespace vigine::eventscheduler
{

/**
 * @brief Pure-virtual facade for the Event dispatch pattern.
 *
 * @ref IEventScheduler is the Level-2 facade over
 * @ref vigine::messaging::IMessageBus for engine-driven timers and OS
 * signals (plan_16, R.3.3.1.2). It encapsulates two operations:
 *
 *   - @ref schedule -- arm a timer or OS-signal watcher; receive an RAII
 *                      handle. When the trigger fires the scheduler posts
 *                      a @ref vigine::messaging::MessageKind::Event message
 *                      to the bound bus addressed to @p target.
 *   - @ref shutdown -- cancel all live events, disarm timers, unsubscribe
 *                      OS signals, and stop accepting new schedules.
 *
 * Ownership: @ref schedule returns a @c std::unique_ptr<IEventHandle> (FF-1).
 * The handle's destructor soft-cancels the event; in-flight deliveries
 * complete. Dropping the handle before the event fires is safe.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-9: factory @ref createEventScheduler returns @c std::unique_ptr.
 *   - INV-10: @c I prefix for this pure-virtual interface (no state).
 *   - INV-11: no graph types (@ref vigine::core::graph::NodeId,
 *             @ref vigine::core::graph::INode, etc.) appear in this header.
 *   - FF-1: @ref schedule returns @c std::unique_ptr<IEventHandle>.
 *
 * The concrete implementation (@ref EventScheduler) is private to
 * @c src/eventscheduler/ and unreachable from public callers.
 */
class IEventScheduler
{
  public:
    virtual ~IEventScheduler() = default;

    /**
     * @brief Arms a timer or OS-signal watcher and binds it to @p target.
     *
     * On each trigger the scheduler posts a
     * @ref vigine::messaging::MessageKind::Event message to the bound bus
     * with @ref vigine::messaging::RouteMode::FirstMatch and @p target as
     * the addressed recipient. When the bus registry no longer contains
     * @p target the fire is logged to the dead-letter channel and the
     * event is disarmed.
     *
     * Returns a non-null @c std::unique_ptr<IEventHandle> on success.
     * Returns a null handle when @p target is null or when @p config is
     * inconsistent (for example neither a positive delay/period nor a
     * valid osSignal).
     *
     * The handle is RAII: destroying it equals calling @ref cancel on it.
     */
    [[nodiscard]] virtual std::unique_ptr<IEventHandle>
        schedule(const EventConfig                          &config,
                 vigine::messaging::AbstractMessageTarget   *target) = 0;

    /**
     * @brief Shuts down the scheduler.
     *
     * Cancels every live event, disarms all timers, unsubscribes all OS
     * signal listeners, and rejects subsequent @ref schedule calls.
     * Idempotent: a second call is a no-op.
     */
    virtual vigine::Result shutdown() = 0;

    IEventScheduler(const IEventScheduler &)            = delete;
    IEventScheduler &operator=(const IEventScheduler &) = delete;
    IEventScheduler(IEventScheduler &&)                 = delete;
    IEventScheduler &operator=(IEventScheduler &&)      = delete;

  protected:
    IEventScheduler() = default;
};

} // namespace vigine::eventscheduler
