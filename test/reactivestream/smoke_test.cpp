#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/reactivestream/defaultreactivestream.h"
#include "vigine/reactivestream/ireactivestream.h"
#include "vigine/reactivestream/ireactivesubscriber.h"
#include "vigine/reactivestream/ireactivesubscription.h"
#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Test suite: ReactiveStream smoke tests (label: reactivestream-smoke)
//
// Scenario 1 — publish / observe round-trip:
//   Subscribe a collector. Signal demand(3). Push 3 payloads. Assert each
//   received. Call complete(). Assert onComplete fired.
//
// Scenario 2 — backpressure: zero demand drops items:
//   Subscribe but do NOT call request(). Push one item. Assert nothing
//   received by the subscriber.
//
// Scenario 3 — cancel stops delivery:
//   Subscribe, request(10), cancel immediately, push one item. Assert
//   nothing received.
//
// Scenario 4 — shutdown signals onComplete to all subscribers:
//   Subscribe two collectors. Call stream.shutdown(). Assert both received
//   onComplete exactly once.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine;
using namespace vigine::messaging;
using namespace vigine::reactivestream;

static constexpr vigine::payload::PayloadTypeId kPayloadId{200};

// ---------------------------------------------------------------------------
// Minimal IMessagePayload.
// ---------------------------------------------------------------------------

class SmokePayload final : public IMessagePayload
{
  public:
    explicit SmokePayload(int tag) noexcept : _tag(tag) {}

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kPayloadId;
    }

    [[nodiscard]] int tag() const noexcept { return _tag; }

  private:
    int _tag;
};

// ---------------------------------------------------------------------------
// CollectorSubscriber — records every onNext payload tag, onComplete, onError.
// ---------------------------------------------------------------------------

class CollectorSubscriber final : public IReactiveSubscriber
{
  public:
    explicit CollectorSubscriber(std::size_t initialDemand = 0)
        : _initialDemand(initialDemand)
    {
    }

    // Receives the subscription and optionally sets initial demand.
    void onSubscribe(std::unique_ptr<IReactiveSubscription> subscription) override
    {
        _subscription = std::move(subscription);
        if (_initialDemand > 0)
        {
            _subscription->request(_initialDemand);
        }
    }

    void onNext(std::unique_ptr<IMessagePayload> payload) override
    {
        if (auto *p = dynamic_cast<SmokePayload *>(payload.get()))
        {
            receivedTags.push_back(p->tag());
        }
        onNextCount.fetch_add(1, std::memory_order_relaxed);
    }

    void onError(vigine::Result /*error*/) override
    {
        onErrorCount.fetch_add(1, std::memory_order_relaxed);
    }

    void onComplete() override
    {
        onCompleteCount.fetch_add(1, std::memory_order_relaxed);
    }

    // Request additional items at any time.
    void request(std::size_t n)
    {
        if (_subscription)
        {
            _subscription->request(n);
        }
    }

    void cancelSubscription()
    {
        if (_subscription)
        {
            _subscription->cancel();
        }
    }

    std::vector<int>   receivedTags;
    std::atomic<int>   onNextCount{0};
    std::atomic<int>   onCompleteCount{0};
    std::atomic<int>   onErrorCount{0};

  private:
    std::unique_ptr<IReactiveSubscription> _subscription;
    std::size_t                            _initialDemand;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ReactiveStreamSmoke : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        _tm = vigine::threading::createThreadManager({});

        BusConfig cfg;
        cfg.threading    = ThreadingPolicy::InlineOnly;
        cfg.backpressure = BackpressurePolicy::Error;
        _bus = createMessageBus(cfg, *_tm);

        _stream = std::make_unique<DefaultReactiveStream>(*_bus, *_tm);
    }

    void TearDown() override
    {
        if (_stream)
        {
            _stream->shutdown();
            _stream.reset();
        }
        if (_bus)
        {
            _bus->shutdown();
        }
        if (_tm)
        {
            _tm->shutdown();
        }
    }

    std::unique_ptr<vigine::threading::IThreadManager> _tm;
    std::unique_ptr<IMessageBus>                        _bus;
    std::unique_ptr<DefaultReactiveStream>              _stream;
};

// ---------------------------------------------------------------------------
// Scenario 1: publish / observe round-trip
//
// Subscribe a collector with demand(3). Push 3 payloads. Assert each was
// received in order. Call complete(). Assert onComplete fired once.
// ---------------------------------------------------------------------------

TEST_F(ReactiveStreamSmoke, PublishObserveRoundTrip)
{
    CollectorSubscriber collector{3};  // initial demand = 3
    auto handle = _stream->subscribe(&collector);
    ASSERT_NE(handle, nullptr) << "subscribe must return a non-null handle";

    // Push 3 payloads.
    EXPECT_TRUE(_stream->publish(std::make_unique<SmokePayload>(1)).isSuccess());
    EXPECT_TRUE(_stream->publish(std::make_unique<SmokePayload>(2)).isSuccess());
    EXPECT_TRUE(_stream->publish(std::make_unique<SmokePayload>(3)).isSuccess());

    EXPECT_EQ(collector.onNextCount.load(), 3)
        << "collector should have received exactly 3 items";
    ASSERT_EQ(collector.receivedTags.size(), 3u);
    EXPECT_EQ(collector.receivedTags[0], 1);
    EXPECT_EQ(collector.receivedTags[1], 2);
    EXPECT_EQ(collector.receivedTags[2], 3);

    // Call complete().
    EXPECT_TRUE(_stream->complete().isSuccess());

    EXPECT_EQ(collector.onCompleteCount.load(), 1)
        << "onComplete should have fired exactly once";
    EXPECT_EQ(collector.onErrorCount.load(), 0)
        << "onError must not fire on a clean completion";
}

// ---------------------------------------------------------------------------
// Scenario 2: zero demand — items are dropped (backpressure)
//
// Subscribe with demand 0. Push one item. Assert nothing received.
// ---------------------------------------------------------------------------

TEST_F(ReactiveStreamSmoke, BackpressureZeroDemandDropsItems)
{
    CollectorSubscriber collector{0};  // no initial demand
    auto handle = _stream->subscribe(&collector);
    ASSERT_NE(handle, nullptr);

    // Push item — no demand, should be silently dropped.
    _stream->publish(std::make_unique<SmokePayload>(99));

    EXPECT_EQ(collector.onNextCount.load(), 0)
        << "subscriber with zero demand must not receive items";
}

// ---------------------------------------------------------------------------
// Scenario 3: cancel stops delivery
//
// Subscribe with demand(10). Cancel immediately. Push one item. Assert
// nothing received.
// ---------------------------------------------------------------------------

TEST_F(ReactiveStreamSmoke, CancelStopsDelivery)
{
    CollectorSubscriber collector{10};
    auto handle = _stream->subscribe(&collector);
    ASSERT_NE(handle, nullptr);

    // Cancel the subscriber's own token.
    collector.cancelSubscription();

    // Push one item — subscription is cancelled, nothing should arrive.
    _stream->publish(std::make_unique<SmokePayload>(7));

    EXPECT_EQ(collector.onNextCount.load(), 0)
        << "cancelled subscriber must not receive items";
}

// ---------------------------------------------------------------------------
// Scenario 4: shutdown signals onComplete to all active subscribers
//
// Subscribe two collectors. Call shutdown(). Assert each receives exactly
// one onComplete.
// ---------------------------------------------------------------------------

TEST_F(ReactiveStreamSmoke, ShutdownSignalsCompleteToAll)
{
    CollectorSubscriber collA{0};
    CollectorSubscriber collB{0};

    auto handleA = _stream->subscribe(&collA);
    auto handleB = _stream->subscribe(&collB);
    ASSERT_NE(handleA, nullptr);
    ASSERT_NE(handleB, nullptr);

    _stream->shutdown();

    EXPECT_EQ(collA.onCompleteCount.load(), 1)
        << "subscriber A must receive exactly one onComplete on shutdown";
    EXPECT_EQ(collB.onCompleteCount.load(), 1)
        << "subscriber B must receive exactly one onComplete on shutdown";

    EXPECT_EQ(collA.onErrorCount.load(), 0);
    EXPECT_EQ(collB.onErrorCount.load(), 0);
}

} // namespace
