// ---------------------------------------------------------------------------
// Scenario 15 -- FF-70 per-subscriber onMessage serialisation.
//
// IMessageBus guarantees that ISubscriber::onMessage is never invoked
// concurrently for the SAME subscriber, even when N producer threads
// drive post() at once on a Shared-policy bus (drain runs on the
// post()-caller thread, so several drains race for the slot's
// per-slot deliverMutex). The contract is documented on
// IMessageBus::post and is the substrate every messaging facade
// relies on for its own thread-safety reasoning.
//
// What this scenario pins
// -----------------------
//   - Spawn N publisher threads on the thread-manager pool. Each
//     publisher posts M envelopes back to back.
//   - One CountingSubscriber is the only consumer for the test's
//     filter; it counts in-flight onMessage entries via an atomic
//     canary. Whenever a second thread enters onMessage while a first
//     is still inside, the canary catches the violation.
//   - After every publisher handle waits to completion, the test
//     asserts:
//       1. The subscriber received exactly N * M dispatches.
//       2. The reentry-violation counter is 0.
//
// The scenario uses a private Shared-policy stack (not the inline-only
// fixture stack) precisely because the FF-70 contract is meaningful
// only when several worker threads can race on a slot. Inline-only
// dispatch is single-threaded by construction and would not exercise
// the deliverMutex.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/factory.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/routemode.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace vigine::contract
{
namespace
{

using SubscriberSerialization = EngineFixture;

constexpr vigine::payload::PayloadTypeId kSerialPayloadTypeId{0x90201u};
constexpr int kPublisherCount  = 8;
constexpr int kMessagesPer     = 100;
constexpr int kExpectedTotal   = kPublisherCount * kMessagesPer;

// Subscriber that counts dispatches and detects concurrent onMessage
// entries on the same slot via an atomic in-flight canary. The "prior"
// value of fetch_add must always be 0 for a slot the bus serialises;
// any non-zero observation increments the violation counter.
class SerialisationSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        const int prior = _inFlight.fetch_add(1, std::memory_order_acq_rel);
        if (prior != 0)
        {
            _violations.fetch_add(1, std::memory_order_acq_rel);
        }
        // A small spin keeps the entry window wide enough that a racing
        // dispatcher would actually observe the in-flight state, without
        // adding a real sleep that would slow the test down.
        for (volatile int i = 0; i < 32; ++i)
        {
            (void)i;
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
        return _violations.load(std::memory_order_acquire);
    }

  private:
    std::atomic<int> _inFlight{0};
    std::atomic<int> _hits{0};
    std::atomic<int> _violations{0};
};

// IRunnable that posts a fixed batch of envelopes to the shared bus.
class BatchPublisher final : public vigine::core::threading::IRunnable
{
  public:
    BatchPublisher(vigine::messaging::IMessageBus &bus, int messages) noexcept
        : _bus(bus)
        , _messages(messages)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        for (int i = 0; i < _messages; ++i)
        {
            const vigine::Result r = _bus.post(std::make_unique<ContractMessage>(
                vigine::messaging::MessageKind::Signal,
                vigine::messaging::RouteMode::FirstMatch,
                kSerialPayloadTypeId));
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

TEST_F(SubscriberSerialization, ConcurrentPublishersNeverReenterOnMessage)
{
    // Build a Shared-policy bus on a fresh thread-manager so the
    // dispatcher actually runs on the post()-caller thread. The fixture
    // stack offers an InlineOnly variant which would not exercise the
    // FF-70 contract (single-threaded by construction).
    //
    // Force at least 4 worker threads regardless of the host CPU count.
    // The factory's default of poolSize == 0 derives the pool from
    // std::thread::hardware_concurrency(), which can report 1 on a
    // single-core CI runner; in that case publishers would run serially
    // and the FF-70 reentry canary would never fire even if the
    // serialisation invariant were broken (a false-pass risk). Pinning
    // poolSize to 4 keeps the contract genuinely under test.
    vigine::core::threading::ThreadManagerConfig tmCfg{};
    tmCfg.poolSize = 4;
    auto tm = vigine::core::threading::createThreadManager(tmCfg);
    ASSERT_NE(tm, nullptr);

    vigine::messaging::BusConfig busCfg{};
    busCfg.name         = "scenario15-serialisation";
    busCfg.priority     = vigine::messaging::BusPriority::Normal;
    busCfg.threading    = vigine::messaging::ThreadingPolicy::Shared;
    busCfg.capacity     = vigine::messaging::QueueCapacity{
        /*maxMessages=*/static_cast<std::size_t>(kExpectedTotal * 2),
        /*bounded=*/true};
    busCfg.backpressure = vigine::messaging::BackpressurePolicy::Block;

    auto bus = vigine::messaging::createMessageBus(busCfg, *tm);
    ASSERT_NE(bus, nullptr);

    SerialisationSubscriber subscriber{};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = kSerialPayloadTypeId;

    auto token = bus->subscribe(filter, &subscriber);
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->active());

    // Schedule kPublisherCount runnables on the worker pool. The
    // handles stay on the stack so the bus + subscriber remain alive
    // until every post() and its triggered drain has returned.
    std::vector<std::unique_ptr<vigine::core::threading::ITaskHandle>> handles;
    handles.reserve(kPublisherCount);

    for (int i = 0; i < kPublisherCount; ++i)
    {
        auto runnable = std::make_unique<BatchPublisher>(*bus, kMessagesPer);
        auto handle   = tm->schedule(
            std::move(runnable),
            vigine::core::threading::ThreadAffinity::Pool);
        ASSERT_NE(handle, nullptr);
        handles.push_back(std::move(handle));
    }

    // Wait every publisher to completion. Each successful wait() means
    // every post() in that runnable returned; with Shared dispatch the
    // drain (and therefore every onMessage call) has returned too.
    for (auto &h : handles)
    {
        const vigine::Result waited = h->waitFor(std::chrono::seconds{30});
        EXPECT_TRUE(waited.isSuccess())
            << "publisher must finish; got: " << waited.message();
    }

    EXPECT_EQ(subscriber.hits(), kExpectedTotal)
        << "subscriber must observe every posted dispatch";
    EXPECT_EQ(subscriber.violations(), 0)
        << "FF-70: onMessage must never run concurrently for the same subscriber";
}

} // namespace
} // namespace vigine::contract
