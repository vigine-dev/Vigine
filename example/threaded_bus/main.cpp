// ---------------------------------------------------------------------------
// example-threaded-bus
//
// Demonstrates that IMessageBus delivers messages from N publisher threads
// to a single subscriber without ever invoking onMessage() concurrently
// for that subscriber. The contract -- a per-slot mutex around the
// onMessage call -- is documented on IMessageBus::post and is the
// substrate every message-driven facade in the engine relies on.
//
// What the demo proves
// --------------------
//   - 8 publisher tasks scheduled on the thread-manager Pool each post()
//     100 messages back-to-back.
//   - The bus uses ThreadingPolicy::Shared so the dispatch drain runs
//     synchronously on each posting worker thread.
//   - A single CountingSubscriber receives every dispatch. It tracks
//     in-flight onMessage calls via an atomic counter; if FF-70 holds,
//     the counter never exceeds 1 and the test reports 0 reentry
//     violations.
//   - All publisher handles wait() to completion before the demo prints
//     its counters; by that point every post() has returned, which
//     means every drain (and therefore every onMessage call) has
//     returned too.
//
// Exit code 0 only when count == 800 AND violations == 0.
// ---------------------------------------------------------------------------

#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/factory.h"
#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/routemode.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace
{

// Picked above the example-window reservation (0x20101 / 0x20102) and
// well clear of the engine-owned ranges so the demo cannot collide with
// any real payload registered elsewhere.
constexpr vigine::payload::PayloadTypeId kTickPayloadTypeId{0x90001u};

constexpr int kPublisherCount  = 8;
constexpr int kMessagesPer     = 100;
constexpr int kExpectedTotal   = kPublisherCount * kMessagesPer;

/**
 * @brief Minimal IMessagePayload carrying only the type id.
 *
 * IMessagePayload is non-copyable / non-movable by design (parent has
 * deleted copy + move). Each post() constructs a fresh payload owned by
 * the enclosing TickMessage so the bus never aliases payload state across
 * envelopes.
 */
class TickPayload final : public vigine::messaging::IMessagePayload
{
  public:
    TickPayload() noexcept = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kTickPayloadTypeId;
    }
};

/**
 * @brief Minimal IMessage envelope wrapping one TickPayload.
 *
 * Mirrors the shape used by the contract test fixture (ContractMessage):
 * Signal kind, FirstMatch route, default correlation + scheduledFor.
 */
class TickMessage final : public vigine::messaging::IMessage
{
  public:
    TickMessage() : _payload(std::make_unique<TickPayload>()) {}

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::Signal;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId
        payloadTypeId() const noexcept override
    {
        return kTickPayloadTypeId;
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *
        payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FirstMatch;
    }

    [[nodiscard]] vigine::messaging::CorrelationId
        correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return std::chrono::steady_clock::time_point{};
    }

  private:
    std::unique_ptr<TickPayload> _payload;
};

/**
 * @brief Subscriber that counts dispatches and detects concurrent
 *        onMessage entries on the same slot.
 *
 * _inFlight is the canary: every onMessage bumps it on entry and
 * decrements on exit. If the bus ever broke FF-70 (per-subscriber
 * serialisation), two threads would observe each other's increments and
 * _reentryViolations would increase. With FF-70 enforced, the counter
 * stays at zero for the whole run.
 */
class CountingSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        // acq_rel + post-load is enough to reason about ordering on a
        // single atomic; the violation counter is a separate atomic so
        // the increment is observable independently of the in-flight
        // counter's value.
        const int prior =
            _inFlight.fetch_add(1, std::memory_order_acq_rel);
        if (prior != 0)
        {
            _reentryViolations.fetch_add(1, std::memory_order_acq_rel);
        }
        _hits.fetch_add(1, std::memory_order_acq_rel);
        _inFlight.fetch_sub(1, std::memory_order_acq_rel);
        return vigine::messaging::DispatchResult::Handled;
    }

    [[nodiscard]] int hits() const noexcept
    {
        return _hits.load(std::memory_order_acquire);
    }

    [[nodiscard]] int violations() const noexcept
    {
        return _reentryViolations.load(std::memory_order_acquire);
    }

  private:
    std::atomic<int> _inFlight{0};
    std::atomic<int> _hits{0};
    std::atomic<int> _reentryViolations{0};
};

/**
 * @brief Runnable that posts kMessagesPer fresh TickMessage envelopes to
 *        the bus.
 *
 * The bus reference is non-owning; the caller's stack keeps the bus
 * alive for the whole publisher fan-out (we wait() on every handle
 * before the bus goes out of scope).
 */
class PublisherRunnable final : public vigine::core::threading::IRunnable
{
  public:
    PublisherRunnable(vigine::messaging::IMessageBus &bus, int messages) noexcept
        : _bus(bus)
        , _messages(messages)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        for (int i = 0; i < _messages; ++i)
        {
            const vigine::Result r =
                _bus.post(std::make_unique<TickMessage>());
            if (r.isError())
            {
                return r;
            }
        }
        return vigine::Result{};
    }

  private:
    vigine::messaging::IMessageBus &_bus;
    int                             _messages;
};

} // namespace

int main()
{
    // Default thread-manager config: pool sized to hardware concurrency.
    auto threadManager =
        vigine::core::threading::createThreadManager(
            vigine::core::threading::ThreadManagerConfig{});
    if (!threadManager)
    {
        std::cerr << "createThreadManager failed\n";
        return 1;
    }

    // Shared-policy bus: dispatch drain runs synchronously on the
    // post()-caller thread. With kPublisherCount workers each calling
    // post() concurrently, several drains race -- FF-70 is what keeps
    // the single-subscriber slot serialised under that race.
    vigine::messaging::BusConfig busConfig{};
    busConfig.name         = "threaded-bus-example";
    busConfig.priority     = vigine::messaging::BusPriority::Normal;
    busConfig.threading    = vigine::messaging::ThreadingPolicy::Shared;
    busConfig.capacity     = vigine::messaging::QueueCapacity{
        /*maxMessages=*/static_cast<std::size_t>(kExpectedTotal * 2),
        /*bounded=*/true};
    busConfig.backpressure = vigine::messaging::BackpressurePolicy::Block;

    auto bus =
        vigine::messaging::createMessageBus(busConfig, *threadManager);
    if (!bus)
    {
        std::cerr << "createMessageBus failed\n";
        return 1;
    }

    CountingSubscriber subscriber{};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = kTickPayloadTypeId;

    auto token = bus->subscribe(filter, &subscriber);
    if (token == nullptr || !token->active())
    {
        std::cerr << "subscribe returned an inert token\n";
        return 1;
    }

    // Schedule kPublisherCount runnables on the worker pool. The handles
    // are kept on the stack so we can wait() on every one before
    // checking the counters; releasing the bus or the subscriber before
    // the last post() returned would be a use-after-free.
    std::vector<std::unique_ptr<vigine::core::threading::ITaskHandle>> handles;
    handles.reserve(kPublisherCount);

    for (int i = 0; i < kPublisherCount; ++i)
    {
        auto runnable =
            std::make_unique<PublisherRunnable>(*bus, kMessagesPer);
        auto handle = threadManager->schedule(
            std::move(runnable),
            vigine::core::threading::ThreadAffinity::Pool);
        if (!handle)
        {
            std::cerr << "schedule returned a null handle for publisher " << i
                      << "\n";
            return 1;
        }
        handles.push_back(std::move(handle));
    }

    // Block until every publisher has finished. Each successful wait()
    // means every post() in that runnable has returned; with the Shared
    // policy that also means every dispatch on that post() has returned.
    bool everyPublisherSucceeded = true;
    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        const vigine::Result r = handles[i]->wait();
        if (r.isError())
        {
            std::cerr << "publisher " << i
                      << " reported an error Result: " << r.message() << "\n";
            everyPublisherSucceeded = false;
        }
    }

    const int hits       = subscriber.hits();
    const int violations = subscriber.violations();

    std::cout << "Received: " << hits << " / " << kExpectedTotal << "\n";
    std::cout << "Reentry violations: " << violations << "\n";

    // Drop the token before the bus / thread manager go out of scope.
    // The token's destructor blocks until any in-flight dispatch on its
    // slot has drained -- by this point that's a no-op, but the explicit
    // ordering documents the contract.
    token.reset();

    const bool ok =
        everyPublisherSucceeded && hits == kExpectedTotal && violations == 0;
    return ok ? 0 : 1;
}
