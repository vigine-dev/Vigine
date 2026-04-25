// ---------------------------------------------------------------------------
// Scenario 16 -- FF-69 token destruction blocks until in-flight onMessage
// drains.
//
// The FF-69 contract is the lifecycle handshake that lets subscribers
// own callback memory without racing the dispatcher: when a caller
// drops the SubscriptionToken (or calls cancel() explicitly), the
// teardown must not return until every onMessage already in flight on
// that slot has returned. The mechanism in AbstractMessageBus is the
// pair of reader/writer locks -- deliver() acquires the slot's
// lifecycleMutex in shared mode for the duration of onMessage, and
// IBusControlBlock::unregisterSubscription() (the call chained from
// the token destructor / cancel()) acquires the same mutex in
// exclusive mode, which blocks until every shared holder releases.
//
// What this scenario pins
// -----------------------
//   1. A subscriber sleeps inside onMessage long enough for the test
//      thread to observe it has entered (signal flag set).
//   2. The test thread waits on that flag, then calls token->cancel().
//      The cancel call must NOT return before the slow onMessage
//      returns -- the test thread captures wall-clock timestamps on
//      both sides of the cancel call.
//   3. The slow handler also captures the wall-clock instant it is
//      about to return. The post-condition is:
//          handler_returned_at <= cancel_returned_at,
//      i.e. the cancel completed at or after the handler released
//      the slot. Conversely, the cancel call must have stayed
//      blocked for "long enough" -- we assert it took at least the
//      handler's sleep duration minus a generous tolerance.
//
// Without FF-69, cancel() would race ahead of the in-flight onMessage
// and the subscriber object could be torn down with a callback still
// running on top of its memory. The contract makes RAII tokens safe;
// this scenario is the regression pin.
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
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace vigine::contract
{
namespace
{

using DtorBlocksInFlight = EngineFixture;

constexpr vigine::payload::PayloadTypeId kSlowPayloadTypeId{0x90301u};
constexpr auto                           kHandlerSleep =
    std::chrono::milliseconds{200};

// Subscriber that signals "entered" on the way in, sleeps for a fixed
// window so the test thread can race a cancel against it, then signals
// "about to return" with a timestamp captured immediately before the
// onMessage call returns.
class SlowSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        // Tell the test thread we are inside the callback. The mutex
        // pairs the flag write with the wait predicate; without the
        // lock the test thread could observe the bool before
        // notify_one's release, which is a benign data race here but
        // not worth diagnosing under TSan.
        {
            std::lock_guard<std::mutex> lock{_mtx};
            _entered = true;
        }
        _cv.notify_one();

        std::this_thread::sleep_for(kHandlerSleep);

        // Capture the instant we are about to release the slot's
        // lifecycleMutex. The test thread captures its own instant
        // either side of cancel(); the relative ordering between
        // those instants is what the FF-69 assertion compares.
        _returnedAt =
            std::chrono::steady_clock::now().time_since_epoch().count();
        return vigine::messaging::DispatchResult::Handled;
    }

    void waitForEnter()
    {
        std::unique_lock<std::mutex> lock{_mtx};
        _cv.wait(lock, [this] { return _entered; });
    }

    [[nodiscard]] long long returnedAt() const noexcept
    {
        return _returnedAt.load(std::memory_order_acquire);
    }

  private:
    std::mutex                _mtx;
    std::condition_variable   _cv;
    bool                      _entered{false};
    std::atomic<long long>    _returnedAt{0};
};

TEST_F(DtorBlocksInFlight, CancelWaitsForInFlightOnMessage)
{
    // Shared-policy bus so dispatch runs synchronously on the
    // post()-caller (a separate poster thread). Inline-only would
    // dispatch on the main thread and we could not race a cancel
    // against the in-flight handler at all.
    vigine::core::threading::ThreadManagerConfig tmCfg{};
    auto tm = vigine::core::threading::createThreadManager(tmCfg);
    ASSERT_NE(tm, nullptr);

    vigine::messaging::BusConfig busCfg{};
    busCfg.name         = "scenario16-dtor-block";
    busCfg.priority     = vigine::messaging::BusPriority::Normal;
    busCfg.threading    = vigine::messaging::ThreadingPolicy::Shared;
    busCfg.capacity     = vigine::messaging::QueueCapacity{16, true};
    busCfg.backpressure = vigine::messaging::BackpressurePolicy::Block;

    auto bus = vigine::messaging::createMessageBus(busCfg, *tm);
    ASSERT_NE(bus, nullptr);

    SlowSubscriber subscriber{};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = kSlowPayloadTypeId;

    auto token = bus->subscribe(filter, &subscriber);
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->active());

    // Poster thread: drives one post() into the bus. Because the bus
    // is Shared-policy, this thread runs the dispatcher synchronously,
    // which means it is the thread that sits inside SlowSubscriber's
    // onMessage for kHandlerSleep.
    std::thread poster{[&bus]() {
        const vigine::Result r = bus->post(std::make_unique<ContractMessage>(
            vigine::messaging::MessageKind::Signal,
            vigine::messaging::RouteMode::FirstMatch,
            kSlowPayloadTypeId));
        EXPECT_TRUE(r.isSuccess()) << "post must succeed; got: " << r.message();
    }};

    // Wait until we know the dispatcher is inside onMessage. From this
    // point cancel() must block on the slot's lifecycle mutex until
    // the handler returns.
    subscriber.waitForEnter();

    const auto cancelStart = std::chrono::steady_clock::now();
    token->cancel();
    const auto cancelEnd = std::chrono::steady_clock::now();

    // The handler captures its own return instant inside onMessage just
    // before returning. The cancel call must observe that instant on
    // its way out: cancelEnd must be at-or-after handlerReturnedAt.
    const long long handlerReturnedNs = subscriber.returnedAt();
    ASSERT_NE(handlerReturnedNs, 0)
        << "handler must have returned by the time cancel observed the gate";

    const auto cancelEndNs = cancelEnd.time_since_epoch().count();
    EXPECT_GE(cancelEndNs, handlerReturnedNs)
        << "FF-69: cancel must not return before in-flight onMessage returns";

    // The cancel call must also have stayed blocked for "long enough".
    // We allow a generous slack so a slow CI box does not flake: at
    // least half the handler-sleep window must have elapsed between
    // cancelStart and cancelEnd (the test thread may have started
    // racing slightly before the handler entered the sleep, but it
    // cannot have finished before the sleep was over).
    const auto blockedFor = cancelEnd - cancelStart;
    EXPECT_GE(blockedFor, kHandlerSleep / 2)
        << "FF-69: cancel must remain blocked while onMessage is in flight";

    poster.join();
}

} // namespace
} // namespace vigine::contract
