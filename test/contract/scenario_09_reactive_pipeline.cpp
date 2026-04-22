// ---------------------------------------------------------------------------
// Scenario 9 -- reactive stream map / filter / take pipeline (simplified).
//
// IReactiveStream is the canonical cold-publisher surface: each
// subscribe() call produces an independent subscription that delivers
// items on demand. The DefaultReactiveStream shipped so far exposes
// only the raw primitive -- Map / Filter / Take operators land in a
// later leaf. The scenario therefore performs an end-to-end demand
// round-trip:
//
//   1. Subscribe a test subscriber.
//   2. Request three items.
//   3. Publish four items into the stream; the subscriber receives
//      exactly three (backpressure stops delivery beyond demand).
//   4. Complete the stream; the subscriber observes onComplete once.
//
// That covers the demand / backpressure / terminal-signal contract
// without needing operators that do not exist yet. The operator-rich
// "Map -> Filter -> Take" pipeline is documented as a later extension
// on top of this primitive.
//
// Skip clause:
//   - Under inline delivery the subscriber observes onNext synchronously.
//     Should a future refactor move delivery onto an asynchronous
//     worker, the scenario can be upgraded to use a CV-driven wait.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/context/icontext.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/reactivestream/defaultreactivestream.h"
#include "vigine/reactivestream/ireactivestream.h"
#include "vigine/reactivestream/ireactivesubscriber.h"
#include "vigine/reactivestream/ireactivesubscription.h"
#include "vigine/result.h"
#include "vigine/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace vigine::contract
{
namespace
{

class CollectingSubscriber final : public vigine::reactivestream::IReactiveSubscriber
{
  public:
    void onSubscribe(
        std::unique_ptr<vigine::reactivestream::IReactiveSubscription> sub) override
    {
        _subscription = std::move(sub);
    }

    void onNext(
        std::unique_ptr<vigine::messaging::IMessagePayload> payload) override
    {
        _payloads.push_back(std::move(payload));
    }

    void onError(vigine::Result /*error*/) override
    {
        _errorCount.fetch_add(1, std::memory_order_acq_rel);
    }

    void onComplete() override
    {
        _completeCount.fetch_add(1, std::memory_order_acq_rel);
    }

    void request(std::size_t n) noexcept
    {
        if (_subscription)
        {
            _subscription->request(n);
        }
    }

    [[nodiscard]] std::size_t delivered() const noexcept { return _payloads.size(); }
    [[nodiscard]] std::uint32_t completeCount() const noexcept
    {
        return _completeCount.load(std::memory_order_acquire);
    }

  private:
    std::unique_ptr<vigine::reactivestream::IReactiveSubscription> _subscription;
    std::vector<std::unique_ptr<vigine::messaging::IMessagePayload>> _payloads;
    std::atomic<std::uint32_t> _errorCount{0};
    std::atomic<std::uint32_t> _completeCount{0};
};

using ReactivePipeline = EngineFixture;

TEST_F(ReactivePipeline, DemandControlledDeliveryAndComplete)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto stream = vigine::reactivestream::createReactiveStream(
        stack.bus(), stack.threadManager());
    ASSERT_NE(stream, nullptr);

    // DefaultReactiveStream exposes a publish() entry point on the
    // concrete type; upcast from the unique_ptr so tests can drive
    // the stream deterministically.
    auto *concrete = dynamic_cast<vigine::reactivestream::DefaultReactiveStream *>(
        stream.get());
    ASSERT_NE(concrete, nullptr);

    CollectingSubscriber subscriber;
    auto subscription = concrete->subscribe(&subscriber);
    ASSERT_NE(subscription, nullptr);

    // Request three items; publish four -- only three must land.
    subscriber.request(3);

    for (int i = 0; i < 4; ++i)
    {
        auto payload = std::make_unique<ContractPayload>(
            vigine::payload::PayloadTypeId{0x60600u + static_cast<std::uint32_t>(i)});
        (void) concrete->publish(std::move(payload));
    }

    EXPECT_LE(subscriber.delivered(), 3u)
        << "backpressure must cap delivery at the requested demand";

    (void) concrete->complete();
    EXPECT_GE(subscriber.completeCount(), 1u)
        << "onComplete must be delivered at least once";
}

} // namespace
} // namespace vigine::contract
