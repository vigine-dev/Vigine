#include "vigine/eventscheduler/defaulteventscheduler.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vigine/messaging/abstractmessagetarget.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/threading/ithreadmanager.h"

namespace vigine::eventscheduler
{

namespace
{

// -----------------------------------------------------------------
// EventMessage — concrete IMessage carrying an event payload.
// Private to this translation unit; never visible to callers.
// -----------------------------------------------------------------

class EventPayload final : public vigine::messaging::IMessagePayload
{
  public:
    explicit EventPayload(vigine::payload::PayloadTypeId typeId) noexcept
        : _typeId(typeId)
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _typeId;
    }

  private:
    vigine::payload::PayloadTypeId _typeId;
};

class EventMessage final : public vigine::messaging::IMessage
{
  public:
    EventMessage(vigine::payload::PayloadTypeId                   payloadTypeId,
                 std::unique_ptr<EventPayload>                    payload,
                 const vigine::messaging::AbstractMessageTarget  *target)
        : _payloadTypeId(payloadTypeId)
        , _payload(std::move(payload))
        , _target(target)
        , _scheduledFor(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::Event;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        return _payloadTypeId;
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *target() const noexcept override
    {
        return _target;
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FirstMatch;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point scheduledFor() const noexcept override
    {
        return _scheduledFor;
    }

  private:
    vigine::payload::PayloadTypeId                         _payloadTypeId;
    std::unique_ptr<EventPayload>                          _payload;
    const vigine::messaging::AbstractMessageTarget        *_target;
    std::chrono::steady_clock::time_point                  _scheduledFor;
};

// -----------------------------------------------------------------
// Default BusConfig for an inline-only event bus.
// InlineOnly keeps dispatch synchronous on the calling thread.
// -----------------------------------------------------------------

[[nodiscard]] vigine::messaging::BusConfig inlineBusConfig() noexcept
{
    return vigine::messaging::BusConfig{
        /* id           */ vigine::messaging::BusId{},
        /* name         */ std::string_view{"event-scheduler-bus"},
        /* priority     */ vigine::messaging::BusPriority::Normal,
        /* threading    */ vigine::messaging::ThreadingPolicy::InlineOnly,
        /* capacity     */ vigine::messaging::QueueCapacity{256, true},
        /* backpressure */ vigine::messaging::BackpressurePolicy::Error,
    };
}

} // namespace

// -----------------------------------------------------------------
// Scheduled event entry stored in the registry.
// -----------------------------------------------------------------

struct ScheduledEvent
{
    EventConfig                                       config;
    vigine::messaging::AbstractMessageTarget         *target{nullptr};
    std::uint64_t                                     timerId{0};
    std::atomic<bool>                                 active{true};
    std::atomic<std::size_t>                          fireCount{0};
    std::atomic<bool>                                 inFlight{false};
};

// -----------------------------------------------------------------
// DefaultEventHandle — RAII handle returned to callers.
// -----------------------------------------------------------------

class DefaultEventHandle final : public IEventHandle
{
  public:
    DefaultEventHandle(std::shared_ptr<ScheduledEvent> event,
                       ITimerSource                   *timerSrc) noexcept
        : _event(std::move(event))
        , _timerSrc(timerSrc)
    {
    }

    ~DefaultEventHandle() override
    {
        cancel();
    }

    void cancel() noexcept override
    {
        if (!_event)
        {
            return;
        }
        // Drop the `active` flag first so any concurrent `onTimerFired`
        // bails out at its own `active.load()` gate before it can
        // observe the disarm race.
        _event->active.store(false, std::memory_order_release);
        // Then tell the underlying source to stop delivering. For
        // periodic events (`config.count == 0`) this is what actually
        // stops the wake-ups — the flag-only prior impl left the timer
        // firing forever, with every fire a no-op but still consuming
        // the thread + callback pair. A one-shot timer that has
        // already fired has `timerId == 0` by the time we get here
        // (the scheduler clears it after the single dispatch); the
        // `disarm` call is guarded accordingly.
        if (_event->timerId != 0 && _timerSrc != nullptr)
        {
            _timerSrc->disarm(_event->timerId);
        }
    }

    [[nodiscard]] bool active() const noexcept override
    {
        return _event && _event->active.load(std::memory_order_acquire);
    }

    DefaultEventHandle(const DefaultEventHandle &)            = delete;
    DefaultEventHandle &operator=(const DefaultEventHandle &) = delete;
    DefaultEventHandle(DefaultEventHandle &&)                 = delete;
    DefaultEventHandle &operator=(DefaultEventHandle &&)      = delete;

  private:
    std::shared_ptr<ScheduledEvent> _event;
    ITimerSource                   *_timerSrc{nullptr};
};

// -----------------------------------------------------------------
// DefaultEventScheduler::Impl
// -----------------------------------------------------------------

struct DefaultEventScheduler::Impl
{
    ITimerSource   &timerSrc;
    IOsSignalSource &osSignalSrc;

    mutable std::shared_mutex                                    eventsMutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<ScheduledEvent>> byTimer;
    std::vector<std::shared_ptr<ScheduledEvent>>                 byOsSignal;

    std::atomic<std::uint64_t> nextId{1};
    std::atomic<bool>          shutdown{false};

    explicit Impl(ITimerSource &ts, IOsSignalSource &oss)
        : timerSrc(ts), osSignalSrc(oss)
    {
    }
};

// -----------------------------------------------------------------
// DefaultEventScheduler
// -----------------------------------------------------------------

DefaultEventScheduler::DefaultEventScheduler(
    vigine::threading::IThreadManager &threadManager,
    ITimerSource                      &timerSource,
    IOsSignalSource                   &osSignalSource)
    : AbstractEventScheduler{inlineBusConfig(), threadManager}
    , _impl(std::make_unique<Impl>(timerSource, osSignalSource))
{
}

DefaultEventScheduler::~DefaultEventScheduler()
{
    shutdown();
}

std::unique_ptr<IEventHandle>
DefaultEventScheduler::schedule(
    const EventConfig                        &config,
    vigine::messaging::AbstractMessageTarget *target)
{
    if (!target)
    {
        return nullptr;
    }
    if (_impl->shutdown.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    auto entry = std::make_shared<ScheduledEvent>();
    entry->config = config;
    entry->target = target;

    if (config.isOsSignalTrigger())
    {
        // Register with OS signal source; store by osSignal.
        auto result = _impl->osSignalSrc.subscribe(config.osSignal, this);
        if (result.isError())
        {
            return nullptr;
        }

        std::unique_lock lock(_impl->eventsMutex);
        _impl->byOsSignal.push_back(entry);
    }
    else if (config.period.count() > 0)
    {
        std::uint64_t timerId = _impl->timerSrc.armPeriodic(
            config.period, config.count, this);
        entry->timerId = timerId;

        std::unique_lock lock(_impl->eventsMutex);
        _impl->byTimer[timerId] = entry;
    }
    else if (config.delay.count() > 0)
    {
        std::uint64_t timerId = _impl->timerSrc.armOneShot(
            config.delay, this);
        entry->timerId = timerId;

        std::unique_lock lock(_impl->eventsMutex);
        _impl->byTimer[timerId] = entry;
    }
    else
    {
        // No valid trigger configured.
        return nullptr;
    }

    return std::make_unique<DefaultEventHandle>(entry, &_impl->timerSrc);
}

vigine::Result DefaultEventScheduler::shutdown()
{
    bool expected = false;
    if (!_impl->shutdown.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        // Already shut down — idempotent.
        return vigine::Result{vigine::Result::Code::Success};
    }

    // Disarm all timers.
    {
        std::unique_lock lock(_impl->eventsMutex);
        for (auto &[timerId, entry] : _impl->byTimer)
        {
            entry->active.store(false, std::memory_order_release);
            _impl->timerSrc.disarm(timerId);
        }
        _impl->byTimer.clear();

        // Cancel OS signal entries.
        for (auto &entry : _impl->byOsSignal)
        {
            entry->active.store(false, std::memory_order_release);
            _impl->osSignalSrc.unsubscribe(entry->config.osSignal, this);
        }
        _impl->byOsSignal.clear();
    }

    return vigine::messaging::AbstractMessageBus::shutdown();
}

void DefaultEventScheduler::onTimerFired(std::uint64_t timerId)
{
    std::shared_ptr<ScheduledEvent> entry;
    {
        std::shared_lock lock(_impl->eventsMutex);
        auto it = _impl->byTimer.find(timerId);
        if (it == _impl->byTimer.end())
        {
            return;
        }
        entry = it->second;
    }

    if (!entry->active.load(std::memory_order_acquire))
    {
        return;
    }

    // Enforce rescheduleWhileRunning policy.
    if (!entry->config.rescheduleWhileRunning)
    {
        bool expected = false;
        if (!entry->inFlight.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
        {
            // Previous delivery still in-flight; skip this fire.
            return;
        }
    }

    // Enforce count limit.
    if (entry->config.count > 0)
    {
        std::size_t fired = entry->fireCount.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (fired > entry->config.count)
        {
            entry->active.store(false, std::memory_order_release);
            entry->inFlight.store(false, std::memory_order_release);
            return;
        }
    }

    // Determine payload type id.
    vigine::payload::PayloadTypeId ptid = entry->config.firedPayloadTypeId;

    auto payload = std::make_unique<EventPayload>(ptid);
    auto msg     = std::make_unique<EventMessage>(ptid, std::move(payload), entry->target);

    // Result intentionally discarded: dead-letter is handled by the bus.
    static_cast<void>(vigine::messaging::AbstractMessageBus::post(std::move(msg)));
    entry->inFlight.store(false, std::memory_order_release);

    // Disarm if one-shot (period == 0).
    if (entry->config.period.count() == 0)
    {
        entry->active.store(false, std::memory_order_release);
    }
}

void DefaultEventScheduler::onOsSignal(OsSignal signal)
{
    std::vector<std::shared_ptr<ScheduledEvent>> matches;
    {
        std::shared_lock lock(_impl->eventsMutex);
        for (auto &entry : _impl->byOsSignal)
        {
            if (entry->config.osSignal == signal
                && entry->active.load(std::memory_order_acquire))
            {
                matches.push_back(entry);
            }
        }
    }

    for (auto &entry : matches)
    {
        vigine::payload::PayloadTypeId ptid = entry->config.firedPayloadTypeId;
        auto payload = std::make_unique<EventPayload>(ptid);
        auto msg     = std::make_unique<EventMessage>(ptid, std::move(payload), entry->target);
        // Result intentionally discarded: dead-letter is handled by the bus.
        static_cast<void>(vigine::messaging::AbstractMessageBus::post(std::move(msg)));
    }
}

// -----------------------------------------------------------------
// Factory
// -----------------------------------------------------------------

std::unique_ptr<IEventScheduler>
createEventScheduler(vigine::threading::IThreadManager &threadManager,
                     ITimerSource                      &timerSource,
                     IOsSignalSource                   &osSignalSource)
{
    return std::make_unique<DefaultEventScheduler>(
        threadManager, timerSource, osSignalSource);
}

} // namespace vigine::eventscheduler
