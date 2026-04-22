#include "vigine/messaging/abstractmessagebus.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "vigine/messaging/abstractmessagetarget.h"
#include "vigine/messaging/connectiontoken.h"
#include "vigine/messaging/iconnectiontoken.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/result.h"
#include "vigine/threading/ithreadmanager.h"

#include "messaging/ibuscontrolblock_default.h"

namespace vigine::messaging
{

// ---------------------------------------------------------------------------
// SubscriptionToken -- RAII handle returned from AbstractMessageBus::subscribe
// ---------------------------------------------------------------------------

AbstractMessageBus::SubscriptionToken::SubscriptionToken(AbstractMessageBus *bus,
                                                         std::uint64_t       serial) noexcept
    : _bus(bus)
    , _serial(serial)
{
}

AbstractMessageBus::SubscriptionToken::~SubscriptionToken()
{
    // Idempotent: the cancel path handles the dead-bus case cleanly.
    cancel();
}

void AbstractMessageBus::SubscriptionToken::cancel() noexcept
{
    if (_cancelled.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }
    if (_bus != nullptr)
    {
        _bus->removeSubscription(_serial);
    }
}

bool AbstractMessageBus::SubscriptionToken::active() const noexcept
{
    // Three-way conjunction, matching the header contract:
    //   1. The token points at a real bus (inert tokens carry
    //      _bus == nullptr and _serial == 0 and are never active).
    //   2. The token has not been cancelled (RAII teardown or an
    //      explicit cancel() flipped the flag).
    //   3. The bus itself has not been shut down. A bus that
    //      finished its shutdown handshake marks every existing
    //      subscription as inert; reporting active == true on a
    //      shut-down bus would invite a caller to unsubscribe a
    //      slot the bus no longer tracks.
    if (_bus == nullptr || _serial == 0)
    {
        return false;
    }
    if (_cancelled.load(std::memory_order_acquire))
    {
        return false;
    }
    return !_bus->_shutdown.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- lifecycle
// ---------------------------------------------------------------------------

AbstractMessageBus::AbstractMessageBus(BusConfig                          config,
                                       vigine::threading::IThreadManager &threadManager)
    : _config(std::move(config))
    , _threadManager(&threadManager)
    , _control(std::make_shared<DefaultBusControlBlock>())
{
}

AbstractMessageBus::~AbstractMessageBus()
{
    // Shutdown is idempotent; running it from the destructor makes sure
    // that a caller who forgets to call shutdown explicitly still sees a
    // clean teardown. The control block is flipped to dead so any
    // outstanding connection tokens become safe no-ops when they unwind.
    (void)shutdown();
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- identity
// ---------------------------------------------------------------------------

BusId AbstractMessageBus::id() const noexcept
{
    return _config.id;
}

const BusConfig &AbstractMessageBus::config() const noexcept
{
    return _config;
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- registerTarget (strong exception guarantee)
// ---------------------------------------------------------------------------

Result AbstractMessageBus::registerTarget(AbstractMessageTarget *target)
{
    if (target == nullptr)
    {
        return Result{Result::Code::InvalidMessageTarget, "null target"};
    }
    if (_shutdown.load(std::memory_order_acquire))
    {
        return Result{Result::Code::Error, "bus already shut down"};
    }
    if (!_control->isAlive())
    {
        return Result{Result::Code::Error, "bus control block is dead"};
    }

    // Allocate the registry slot first so a failure past this point
    // only needs to roll back the allocation, not a half-built target
    // connection. allocateSlot returns the sentinel id on failure; we
    // translate that into an error result without mutating any state on
    // the target side.
    const ConnectionId connectionId = _control->allocateSlot(target);
    if (!connectionId.valid())
    {
        return Result{Result::Code::Error, "bus registry exhausted or dead"};
    }

    // Rollback guard: if make_unique below or acceptConnection throws,
    // we must unregister the slot so the registry ends byte-identical
    // to its pre-call state (strong exception guarantee, Q-CT4).
    std::unique_ptr<IConnectionToken> token;
    try
    {
        token = std::make_unique<ConnectionToken>(
            std::weak_ptr<IBusControlBlock>(_control), connectionId);
    }
    catch (...)
    {
        _control->unregisterTarget(connectionId);
        throw;
    }

    try
    {
        target->acceptConnection(std::move(token));
    }
    catch (...)
    {
        // acceptConnection may throw on vector reallocation. Roll the
        // registry back and rethrow so callers see the original
        // exception type; the token unique_ptr will have been consumed
        // if acceptConnection made it past its own locked section, but
        // when it throws on push_back the token is destroyed along with
        // the argument -- which calls unregisterTarget through the
        // ConnectionToken destructor. Calling unregisterTarget a second
        // time is a documented no-op.
        _control->unregisterTarget(connectionId);
        throw;
    }

    return Result{};
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- post
// ---------------------------------------------------------------------------

namespace
{
/// @brief Validates that the message carries a closed-enum kind.
[[nodiscard]] bool validKind(MessageKind kind) noexcept
{
    switch (kind)
    {
        case MessageKind::Signal:
        case MessageKind::Event:
        case MessageKind::TopicPublish:
        case MessageKind::TopicRequest:
        case MessageKind::ChannelSend:
        case MessageKind::ReactiveSignal:
        case MessageKind::ActorMail:
        case MessageKind::PipelineStep:
        case MessageKind::Control:
            return true;
    }
    return false;
}

/// @brief Validates that the message carries a closed-enum route mode.
[[nodiscard]] bool validRouteMode(RouteMode mode) noexcept
{
    switch (mode)
    {
        case RouteMode::FirstMatch:
        case RouteMode::FanOut:
        case RouteMode::Chain:
        case RouteMode::Bubble:
        case RouteMode::Broadcast:
            return true;
    }
    return false;
}
} // namespace

Result AbstractMessageBus::post(std::unique_ptr<IMessage> message)
{
    if (!message)
    {
        return Result{Result::Code::Error, "null message"};
    }
    if (_shutdown.load(std::memory_order_acquire))
    {
        return Result{Result::Code::Error, "bus already shut down"};
    }
    if (!validKind(message->kind()))
    {
        return Result{Result::Code::Error, "invalid MessageKind"};
    }
    if (!validRouteMode(message->routeMode()))
    {
        return Result{Result::Code::Error, "invalid RouteMode"};
    }

    // InlineOnly: dispatch synchronously on the caller's thread. No
    // queue, no worker; the bus acts as a thin fan-out over the
    // registry. Used by tests and embedded setups that want a
    // deterministic single-threaded dispatch.
    if (_config.threading == ThreadingPolicy::InlineOnly)
    {
        dispatchOne(*message);
        return Result{};
    }

    const auto deadline = message->scheduledFor();
    QueuedMessage queued{std::move(message), deadline};

    {
        std::unique_lock<std::mutex> lock{_queueMutex};

        if (_config.capacity.bounded)
        {
            const std::size_t cap = _config.capacity.maxMessages == 0
                                      ? std::size_t{1}
                                      : _config.capacity.maxMessages;
            if (_queue.size() >= cap)
            {
                switch (_config.backpressure)
                {
                    case BackpressurePolicy::Block:
                        _queueCv.wait(lock, [&] {
                            return _queue.size() < cap
                                   || _shutdown.load(std::memory_order_acquire);
                        });
                        if (_shutdown.load(std::memory_order_acquire))
                        {
                            return Result{Result::Code::Error, "bus shut down during post"};
                        }
                        break;
                    case BackpressurePolicy::DropOldest:
                        _queue.pop_front();
                        break;
                    case BackpressurePolicy::Error:
                        return Result{Result::Code::Error, "queue full"};
                }
            }
        }

        _queue.push_back(std::move(queued));
    }

    _queueCv.notify_one();

    // Dispatch inline for the simple single-threaded test path. A full
    // worker-thread pump that drains the queue through the injected
    // thread manager is documented in plan_09 but intentionally left
    // for the lifecycle leaf (plan_23) to wire up -- this leaf ships
    // the dispatch core and the queue buffer, and relies on the
    // caller's thread for drain. Tests under ThreadingPolicy::Shared
    // should poll drainQueue(false) after each post; the system bus
    // with ThreadingPolicy::Dedicated is wired to a dedicated runnable
    // by the engine at its assembly site.
    drainQueue(false);

    return Result{};
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- subscribe / unsubscribe
// ---------------------------------------------------------------------------

std::unique_ptr<ISubscriptionToken>
AbstractMessageBus::subscribe(const MessageFilter &filter, ISubscriber *subscriber)
{
    if (subscriber == nullptr || _shutdown.load(std::memory_order_acquire)
        || !validKind(filter.kind))
    {
        // Inert token: active() always false, destructor is a no-op
        // because the serial points at no live slot.
        return std::make_unique<SubscriptionToken>(nullptr, 0);
    }

    std::uint64_t serial = 0;
    {
        std::unique_lock<std::shared_mutex> lock{_registryMutex};
        serial = _nextSerial++;
        SubscriptionSlot slot{};
        slot.subscriber = subscriber;
        slot.filter     = filter;
        slot.serial     = serial;
        slot.active     = true;
        _subscriptions.emplace(serial, std::move(slot));
    }

    return std::make_unique<SubscriptionToken>(this, serial);
}

void AbstractMessageBus::removeSubscription(std::uint64_t serial) noexcept
{
    if (serial == 0)
    {
        return;
    }
    std::unique_lock<std::shared_mutex> lock{_registryMutex};
    auto it = _subscriptions.find(serial);
    if (it == _subscriptions.end())
    {
        return;
    }
    _subscriptions.erase(it);
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- shutdown
// ---------------------------------------------------------------------------

Result AbstractMessageBus::shutdown()
{
    // Idempotent. Flip the flag first so that any thread racing with us
    // observes the dead state before we start draining.
    const bool already = _shutdown.exchange(true, std::memory_order_acq_rel);
    if (already)
    {
        return Result{};
    }

    // Wake any producer blocked on backpressure so it can exit with the
    // shutdown result rather than block forever.
    {
        std::lock_guard<std::mutex> lock{_queueMutex};
        (void)lock;
    }
    _queueCv.notify_all();

    // Drain the remaining queue. Workers that are still running will
    // pick up the shutdown flag on their next loop iteration and exit.
    drainQueue(true);

    // Clear the subscription registry so that subsequent subscribe
    // calls return inert tokens and existing SubscriptionToken
    // destructors find nothing to remove.
    {
        std::unique_lock<std::shared_mutex> lock{_registryMutex};
        _subscriptions.clear();
    }

    // Mark the control block dead so any outstanding ConnectionToken
    // destructors become safe no-ops.
    if (_control)
    {
        _control->markDead();
    }

    return Result{};
}

// ---------------------------------------------------------------------------
// AbstractMessageBus -- dispatch helpers
// ---------------------------------------------------------------------------

std::vector<AbstractMessageBus::SubscriptionSlot>
AbstractMessageBus::snapshotRegistry() const
{
    std::vector<SubscriptionSlot> snapshot;
    std::shared_lock<std::shared_mutex> lock{_registryMutex};
    snapshot.reserve(_subscriptions.size());
    for (const auto &entry : _subscriptions)
    {
        if (entry.second.active)
        {
            snapshot.push_back(entry.second);
        }
    }
    return snapshot;
}

void AbstractMessageBus::drainQueue(bool untilShutdown)
{
    while (true)
    {
        QueuedMessage item;
        bool          haveItem = false;
        {
            std::unique_lock<std::mutex> lock{_queueMutex};
            if (_queue.empty())
            {
                if (!untilShutdown)
                {
                    return;
                }
                // untilShutdown path: exit once the shutdown flag is
                // set and the queue is drained. The flag was flipped
                // by shutdown() before this call, so the queue is the
                // only thing holding us here.
                if (_shutdown.load(std::memory_order_acquire))
                {
                    return;
                }
            }
            if (_queue.empty())
            {
                _queueCv.wait(lock, [&] {
                    return !_queue.empty() || _shutdown.load(std::memory_order_acquire);
                });
                if (_queue.empty())
                {
                    return;
                }
            }

            // Look for the first ready-to-dispatch item. Future-
            // scheduled messages (deadline > now) stay in place and
            // wait for a later drain pass — the previous impl popped
            // the head, saw it was future, pushed it to the back,
            // and returned, which starved every later-ready message
            // behind a single future-scheduled one (and, for
            // untilShutdown = true paths, never completed the drain).
            const auto now = std::chrono::steady_clock::now();
            for (auto it = _queue.begin(); it != _queue.end(); ++it)
            {
                if (it->deadline <= now)
                {
                    item     = std::move(*it);
                    _queue.erase(it);
                    haveItem = true;
                    break;
                }
            }

            if (!haveItem)
            {
                // Queue carries only future-scheduled work. Return so
                // the caller can re-poll (drainQueue is called from
                // post() and from shutdown()); spinning here would
                // burn CPU on deadlines that have not arrived.
                return;
            }
        }
        _queueCv.notify_one();

        if (item.message)
        {
            dispatchOne(*item.message);
        }
    }
}

DispatchResult AbstractMessageBus::deliver(ISubscriber &subscriber,
                                           const IMessage &message) noexcept
{
    try
    {
        return subscriber.onMessage(message);
    }
    catch (const std::exception &ex)
    {
        // Exception isolation at the dispatch boundary. A misbehaving
        // subscriber must not stall the whole registry. The header
        // advertises that an escape is logged and swallowed; emit a
        // minimal diagnostic on stderr so the silent-failure mode the
        // catch used to produce no longer hides the root cause from
        // whoever is debugging a missed delivery. A proper logging
        // hook will replace the stderr call when the engine
        // standardises one.
        std::fprintf(stderr,
                     "[vigine::messaging] subscriber onMessage threw: %s\n",
                     ex.what());
        return DispatchResult::Handled;
    }
    catch (...)
    {
        std::fprintf(stderr,
                     "[vigine::messaging] subscriber onMessage threw a "
                     "non-std::exception object\n");
        return DispatchResult::Handled;
    }
}

bool AbstractMessageBus::matches(const SubscriptionSlot &slot,
                                 const IMessage         &message) noexcept
{
    if (!slot.active)
    {
        return false;
    }
    if (slot.filter.kind != message.kind())
    {
        return false;
    }
    if (slot.filter.typeId.value != 0
        && slot.filter.typeId != message.payloadTypeId())
    {
        return false;
    }
    if (slot.filter.expectedRoute.has_value()
        && *slot.filter.expectedRoute != message.routeMode())
    {
        return false;
    }
    return true;
}

void AbstractMessageBus::dispatchOne(const IMessage &message)
{
    const auto snapshot = snapshotRegistry();
    switch (message.routeMode())
    {
        case RouteMode::FirstMatch:
            dispatchFirstMatch(message, snapshot);
            break;
        case RouteMode::FanOut:
            dispatchFanOut(message, snapshot);
            break;
        case RouteMode::Chain:
            dispatchChain(message, snapshot);
            break;
        case RouteMode::Bubble:
            dispatchBubble(message, snapshot);
            break;
        case RouteMode::Broadcast:
            dispatchBroadcast(message, snapshot);
            break;
    }
}

} // namespace vigine::messaging
