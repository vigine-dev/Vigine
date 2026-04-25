#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/connectiontoken.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/ibuscontrolblock.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/messaging/subscriptionslot.h"
#include "vigine/messaging/systemmessagebus.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>

namespace
{

using namespace vigine;
using namespace vigine::messaging;

// ---------------------------------------------------------------------------
// Test doubles: minimal concrete IMessagePayload / IMessage / ISubscriber.
// ---------------------------------------------------------------------------

class SmokePayload final : public IMessagePayload
{
  public:
    explicit SmokePayload(vigine::payload::PayloadTypeId id) noexcept : _id(id) {}
    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _id;
    }

  private:
    vigine::payload::PayloadTypeId _id;
};

class SmokeMessage final : public IMessage
{
  public:
    SmokeMessage(MessageKind                    kind,
                 RouteMode                      route,
                 vigine::payload::PayloadTypeId id) noexcept
        : _kind(kind)
        , _route(route)
        , _id(id)
        , _payload(std::make_unique<SmokePayload>(id))
    {
    }

    [[nodiscard]] MessageKind kind() const noexcept override { return _kind; }
    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        return _id;
    }
    [[nodiscard]] const IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }
    [[nodiscard]] const AbstractMessageTarget *target() const noexcept override
    {
        return nullptr;
    }
    [[nodiscard]] RouteMode routeMode() const noexcept override { return _route; }
    [[nodiscard]] CorrelationId correlationId() const noexcept override
    {
        return CorrelationId{};
    }
    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return std::chrono::steady_clock::time_point{};
    }

  private:
    MessageKind                      _kind;
    RouteMode                        _route;
    vigine::payload::PayloadTypeId   _id;
    std::unique_ptr<IMessagePayload> _payload;
};

class CountingSubscriber final : public ISubscriber
{
  public:
    explicit CountingSubscriber(DispatchResult reply) noexcept : _reply(reply) {}

    [[nodiscard]] DispatchResult onMessage(const IMessage & /*message*/) override
    {
        _hits.fetch_add(1, std::memory_order_acq_rel);
        return _reply;
    }

    [[nodiscard]] std::uint32_t hits() const noexcept
    {
        return _hits.load(std::memory_order_acquire);
    }

  private:
    DispatchResult             _reply;
    std::atomic<std::uint32_t> _hits{0};
};

class ThrowingSubscriber final : public ISubscriber
{
  public:
    [[nodiscard]] DispatchResult onMessage(const IMessage & /*message*/) override
    {
        _calls.fetch_add(1, std::memory_order_acq_rel);
        throw std::runtime_error{"boom"};
    }

    [[nodiscard]] std::uint32_t calls() const noexcept
    {
        return _calls.load(std::memory_order_acquire);
    }

  private:
    std::atomic<std::uint32_t> _calls{0};
};

// ---------------------------------------------------------------------------
// Test fixture. Creates an InlineOnly bus over a minimal thread manager so
// every test dispatches synchronously on the calling thread -- which keeps
// the smoke suite deterministic and short.
// ---------------------------------------------------------------------------

struct MessagingSmoke : public ::testing::Test
{
    void SetUp() override
    {
        _threadManager = vigine::core::threading::createThreadManager(
            vigine::core::threading::ThreadManagerConfig{});
        ASSERT_TRUE(_threadManager);
    }

    void TearDown() override
    {
        _threadManager.reset();
    }

    [[nodiscard]] BusConfig inlineConfig(std::string_view name = "smoke-bus") const noexcept
    {
        BusConfig cfg{};
        cfg.name         = name;
        cfg.priority     = BusPriority::Normal;
        cfg.threading    = ThreadingPolicy::InlineOnly;
        cfg.capacity     = QueueCapacity{64, true};
        cfg.backpressure = BackpressurePolicy::Block;
        return cfg;
    }

    std::unique_ptr<vigine::core::threading::IThreadManager> _threadManager;
};

// ---------------------------------------------------------------------------
// Case 1 -- factory returns a unique_ptr<IMessageBus> (FF-1 contract).
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, FactoryReturnsUniquePtr)
{
    std::unique_ptr<IMessageBus> bus = createMessageBus(inlineConfig(), *_threadManager);
    ASSERT_NE(bus, nullptr);
    EXPECT_TRUE(bus->id().valid());
    EXPECT_EQ(bus->config().name, std::string_view{"smoke-bus"});
}

// ---------------------------------------------------------------------------
// Case 2 -- FirstMatch delivers to exactly one matching subscriber.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, FirstMatchEarlyExit)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber a{DispatchResult::Handled};
    CountingSubscriber b{DispatchResult::Handled};

    MessageFilter filter{};
    filter.kind = MessageKind::Signal;

    auto ta = bus->subscribe(filter, &a);
    auto tb = bus->subscribe(filter, &b);
    ASSERT_TRUE(ta && tb);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Signal,
        RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{0x10100u}));
    EXPECT_TRUE(posted.isSuccess());

    const auto total = a.hits() + b.hits();
    EXPECT_EQ(total, 1u);
}

// ---------------------------------------------------------------------------
// Case 3 -- FanOut delivers to every matching subscriber exactly once.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, FanOutEveryMatchingSubscriber)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber a{DispatchResult::Pass};
    CountingSubscriber b{DispatchResult::Pass};
    CountingSubscriber c{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::TopicPublish;

    auto ta = bus->subscribe(filter, &a);
    auto tb = bus->subscribe(filter, &b);
    auto tc = bus->subscribe(filter, &c);
    ASSERT_TRUE(ta && tb && tc);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::TopicPublish,
        RouteMode::FanOut,
        vigine::payload::PayloadTypeId{0x10200u}));
    EXPECT_TRUE(posted.isSuccess());

    EXPECT_EQ(a.hits(), 1u);
    EXPECT_EQ(b.hits(), 1u);
    EXPECT_EQ(c.hits(), 1u);
}

// ---------------------------------------------------------------------------
// Case 4 -- Chain walks until a subscriber reports Handled.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, ChainStopsOnHandled)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber a{DispatchResult::Pass};
    CountingSubscriber b{DispatchResult::Handled};
    CountingSubscriber c{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::PipelineStep;

    auto ta = bus->subscribe(filter, &a);
    auto tb = bus->subscribe(filter, &b);
    auto tc = bus->subscribe(filter, &c);
    ASSERT_TRUE(ta && tb && tc);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::PipelineStep,
        RouteMode::Chain,
        vigine::payload::PayloadTypeId{0x10300u}));
    EXPECT_TRUE(posted.isSuccess());

    EXPECT_EQ(a.hits(), 1u);
    EXPECT_EQ(b.hits(), 1u);
    EXPECT_EQ(c.hits(), 0u);
}

// ---------------------------------------------------------------------------
// Case 4b -- Bubble stops on the first subscriber that reports Handled.
//
// The v1 AbstractMessageTarget has no parent() hook, so Bubble
// currently degrades to FirstMatch-with-Handled semantics: walk the
// snapshot in registration order, deliver to the first matching
// subscriber, stop when it reports Handled or Stop. This smoke case
// pins that shipped shape — when parent chains land in a later
// leaf, the test shape grows to cover walking upward.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, BubbleStopsOnHandled)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber first{DispatchResult::Handled};
    CountingSubscriber second{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::Event;

    auto tfirst  = bus->subscribe(filter, &first);
    auto tsecond = bus->subscribe(filter, &second);
    ASSERT_TRUE(tfirst && tsecond);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Event,
        RouteMode::Bubble,
        vigine::payload::PayloadTypeId{0x10350u}));
    EXPECT_TRUE(posted.isSuccess());

    // First matching subscriber sees the message once and reports
    // Handled; the walk stops before reaching `second`.
    EXPECT_EQ(first.hits(), 1u);
    EXPECT_EQ(second.hits(), 0u);
}

// ---------------------------------------------------------------------------
// Case 5 -- Broadcast reaches every subscriber regardless of filter target.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, BroadcastReachesEveryone)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber a{DispatchResult::Pass};
    CountingSubscriber b{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::Control;

    auto ta = bus->subscribe(filter, &a);
    auto tb = bus->subscribe(filter, &b);
    ASSERT_TRUE(ta && tb);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Control,
        RouteMode::Broadcast,
        vigine::payload::PayloadTypeId{0x00000010u}));
    EXPECT_TRUE(posted.isSuccess());

    EXPECT_EQ(a.hits(), 1u);
    EXPECT_EQ(b.hits(), 1u);
}

// ---------------------------------------------------------------------------
// Case 6 -- Subscription token cancels cleanly (no more deliveries).
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, TokenCancelStopsDelivery)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    CountingSubscriber a{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::Event;

    auto token = bus->subscribe(filter, &a);
    ASSERT_TRUE(token);
    EXPECT_TRUE(token->active());
    token->cancel();
    EXPECT_FALSE(token->active());

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Event,
        RouteMode::Broadcast,
        vigine::payload::PayloadTypeId{0x10400u}));
    EXPECT_TRUE(posted.isSuccess());

    EXPECT_EQ(a.hits(), 0u);
}

// ---------------------------------------------------------------------------
// Case 7 -- Shutdown drains and rejects subsequent posts.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, ShutdownDrainsAndRejectsPost)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    const Result shutdown = bus->shutdown();
    EXPECT_TRUE(shutdown.isSuccess());

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Signal,
        RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{0x10500u}));
    EXPECT_TRUE(posted.isError());
}

// ---------------------------------------------------------------------------
// Case 8 -- Exception isolation: throwing subscriber does not stall others.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, ThrowingSubscriberIsolated)
{
    auto bus = createMessageBus(inlineConfig(), *_threadManager);

    ThrowingSubscriber  thrower{};
    CountingSubscriber  follower{DispatchResult::Pass};

    MessageFilter filter{};
    filter.kind = MessageKind::Signal;

    auto t1 = bus->subscribe(filter, &thrower);
    auto t2 = bus->subscribe(filter, &follower);
    ASSERT_TRUE(t1 && t2);

    const Result posted = bus->post(std::make_unique<SmokeMessage>(
        MessageKind::Signal,
        RouteMode::FanOut,
        vigine::payload::PayloadTypeId{0x10600u}));
    EXPECT_TRUE(posted.isSuccess());

    EXPECT_EQ(thrower.calls(), 1u);
    EXPECT_EQ(follower.hits(), 1u);
}

// ---------------------------------------------------------------------------
// Case 9 -- Token cancel blocks until any in-flight onMessage returns.
//
// The ISubscriptionToken contract says: "The destructor blocks until every
// in-flight dispatch targeting this slot has returned."  This test pins that
// guarantee by posting a message from a second thread whose subscriber sleeps
// for a fixed interval.  The main thread cancels the token concurrently and
// checks that cancel() did not return before the subscriber finished.
// ---------------------------------------------------------------------------

/// @brief Subscriber that sleeps inside onMessage and records when it enters
///        and exits the call so the test can assert cancel() timing.
class SlowSubscriber final : public ISubscriber
{
  public:
    explicit SlowSubscriber(std::chrono::milliseconds delay) noexcept
        : _delay(delay)
    {
    }

    [[nodiscard]] DispatchResult onMessage(const IMessage & /*message*/) override
    {
        // Signal that onMessage has started and record the entry time.
        _entered.store(true, std::memory_order_release);
        _enteredCv.notify_all();

        std::this_thread::sleep_for(_delay);

        _exitedAt = std::chrono::steady_clock::now();
        _exited.store(true, std::memory_order_release);
        return DispatchResult::Handled;
    }

    void waitForEntry(std::chrono::milliseconds timeout = std::chrono::milliseconds{500}) const
    {
        std::unique_lock<std::mutex> lk{_cvMutex};
        _enteredCv.wait_for(lk, timeout, [this] {
            return _entered.load(std::memory_order_acquire);
        });
    }

    [[nodiscard]] bool exited() const noexcept
    {
        return _exited.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::chrono::steady_clock::time_point exitedAt() const noexcept
    {
        return _exitedAt;
    }

  private:
    std::chrono::milliseconds              _delay;
    std::atomic<bool>                      _entered{false};
    std::atomic<bool>                      _exited{false};
    std::chrono::steady_clock::time_point  _exitedAt{};
    mutable std::mutex                     _cvMutex;
    mutable std::condition_variable        _enteredCv;
};

TEST_F(MessagingSmoke, TokenCancelBlocksUntilInFlightDispatchDrains)
{
    auto bus = createMessageBus(inlineConfig("dtor-blocks-bus"), *_threadManager);

    // Use a 50 ms sleep so the window is wide enough to catch races
    // without making the test slow.
    const auto delay = std::chrono::milliseconds{50};
    SlowSubscriber slow{delay};

    MessageFilter filter{};
    filter.kind = MessageKind::Signal;

    auto token = bus->subscribe(filter, &slow);
    ASSERT_TRUE(token);
    ASSERT_TRUE(token->active());

    // Post the message from a background thread so that `onMessage` runs
    // concurrently with the main thread's cancel() call.  InlineOnly
    // dispatches synchronously on the post() caller's thread.
    std::thread dispatcher([&bus] {
        (void)bus->post(std::make_unique<SmokeMessage>(
            MessageKind::Signal,
            RouteMode::Broadcast,
            vigine::payload::PayloadTypeId{0x10700u}));
    });

    // Wait until `onMessage` has actually started before calling cancel()
    // so we know the dispatch is in-flight when we cancel.
    slow.waitForEntry();

    const auto cancelStart = std::chrono::steady_clock::now();
    token->cancel();
    const auto cancelEnd = std::chrono::steady_clock::now();

    // cancel() must have waited: it should return no earlier than when
    // the subscriber's sleep finished.
    EXPECT_TRUE(slow.exited())
        << "onMessage had not returned when cancel() returned — dtor-blocks "
           "contract violated";

    if (slow.exited())
    {
        // Allow a small margin for OS scheduling jitter.
        EXPECT_GE(cancelEnd, slow.exitedAt())
            << "cancel() returned before onMessage() exited";
    }

    dispatcher.join();
}

// ---------------------------------------------------------------------------
// Case 10 -- ConnectionToken::cancel() is idempotent and flips active() false.
//
// Pins the API-symmetry contract L-B2 adds alongside
// ISubscriptionToken::cancel():
//
//   1. active() reports true while the slot is live.
//   2. The first cancel() unregisters the slot AND trips the shared
//      SlotState's `cancelled` flag (the registry observes the loss).
//   3. active() reports false after cancel().
//   4. A second cancel() is a structural no-op -- the control block
//      sees exactly one unregisterTarget call across both invocations.
//
// Uses an in-test fake IBusControlBlock so the assertion is local: the
// ConnectionToken is constructed directly and observed directly,
// without routing through the full AbstractMessageBus register path
// (which hides the token inside AbstractMessageTarget's private
// _connections vector).
// ---------------------------------------------------------------------------

/// @brief Minimal IBusControlBlock that counts unregisterTarget calls
///        and tracks whether the last unregister saw its slot live.
///
/// Everything else is either a pass-through or a no-op; the test only
/// cares about unregister bookkeeping plus the two contracts the block
/// must honour for ConnectionToken to behave correctly: @ref isAlive
/// returns true until @ref markDead runs, and @ref allocateSlot hands
/// back a valid id paired with a fresh SlotState. Subscription-side
/// methods are stubbed because ConnectionToken never reaches them.
class FakeBusControlBlock final : public IBusControlBlock,
                                  public std::enable_shared_from_this<FakeBusControlBlock>
{
  public:
    [[nodiscard]] bool isAlive() const noexcept override
    {
        return _alive.load(std::memory_order_acquire);
    }

    void markDead() noexcept override
    {
        _alive.store(false, std::memory_order_release);
    }

    [[nodiscard]] SlotAllocation
        allocateSlot(AbstractMessageTarget * /*target*/) override
    {
        if (!_alive.load(std::memory_order_acquire))
        {
            return SlotAllocation{};
        }
        auto state = std::make_shared<SlotState>();
        _lastState = state;
        return SlotAllocation{
            ConnectionId{_nextIndex++, _nextGeneration++},
            std::move(state),
        };
    }

    void unregisterTarget(ConnectionId id) noexcept override
    {
        if (!id.valid())
        {
            return;
        }
        _unregisterCalls.fetch_add(1, std::memory_order_acq_rel);
        _lastUnregisteredId = id;
    }

    [[nodiscard]] std::uint64_t registerSubscription(
        ISubscriber * /*subscriber*/,
        MessageFilter /*filter*/,
        std::shared_ptr<SlotState> /*slotState*/) override
    {
        return 0;
    }

    void unregisterSubscription(std::uint64_t /*serial*/) noexcept override {}

    [[nodiscard]] std::vector<SubscriptionSlot> snapshotSubscriptions() const override
    {
        return {};
    }

    [[nodiscard]] std::uint32_t unregisterCalls() const noexcept
    {
        return _unregisterCalls.load(std::memory_order_acquire);
    }

    [[nodiscard]] ConnectionId lastUnregisteredId() const noexcept
    {
        return _lastUnregisteredId;
    }

    [[nodiscard]] std::shared_ptr<SlotState> lastState() const noexcept
    {
        return _lastState;
    }

  private:
    std::atomic<bool>           _alive{true};
    std::atomic<std::uint32_t>  _unregisterCalls{0};
    std::uint32_t               _nextIndex{1};
    std::uint32_t               _nextGeneration{1};
    ConnectionId                _lastUnregisteredId{};
    std::shared_ptr<SlotState>  _lastState;
};

TEST_F(MessagingSmoke, ConnectionTokenCancelIsIdempotent)
{
    auto block       = std::make_shared<FakeBusControlBlock>();
    auto allocation  = block->allocateSlot(nullptr);
    ASSERT_TRUE(allocation.id.valid());
    ASSERT_NE(allocation.state, nullptr);

    ConnectionToken token{
        std::weak_ptr<IBusControlBlock>(block),
        allocation.id,
        allocation.state,
    };

    // Preconditions: the fresh token is live, no unregister yet.
    EXPECT_TRUE(token.active());
    EXPECT_EQ(block->unregisterCalls(), 0u);
    EXPECT_FALSE(allocation.state->cancelled);

    // First cancel: runs the full unregister-plus-barrier sequence.
    token.cancel();

    EXPECT_FALSE(token.active())
        << "active() must report false after the first cancel()";
    EXPECT_EQ(block->unregisterCalls(), 1u)
        << "first cancel() must drive exactly one unregisterTarget";
    EXPECT_EQ(block->lastUnregisteredId(), allocation.id)
        << "unregisterTarget must be called with the token's own id";
    EXPECT_TRUE(allocation.state->cancelled)
        << "the shared SlotState's cancelled flag must be true after cancel()";

    // Second cancel: idempotent no-op. The control block must not see
    // a second unregisterTarget and the state must stay cancelled.
    token.cancel();

    EXPECT_FALSE(token.active());
    EXPECT_EQ(block->unregisterCalls(), 1u)
        << "second cancel() must not drive another unregisterTarget";
    EXPECT_TRUE(allocation.state->cancelled);

    // A third cancel: same invariants as the second.
    token.cancel();
    EXPECT_EQ(block->unregisterCalls(), 1u);
}

// ---------------------------------------------------------------------------
// Case 11 -- ConnectionToken::active() honours an explicit unregister.
//
// Pins the L-B6 contract: once a token has been cancelled (which
// trips its own atomic _cancelled flag AND, on the way through, the
// shared SlotState->cancelled flag under lifecycleMutex), active()
// must report false even though the FakeBusControlBlock is still
// alive and reachable through the weak_ptr. Without the new
// _cancelled / _slotState->cancelled short-circuits inside
// ConnectionToken::active(), the call would still walk all the way
// down to ctrl->isAlive() and return true.
// ---------------------------------------------------------------------------

TEST_F(MessagingSmoke, ConnectionTokenActiveHonorsCancel)
{
    auto block      = std::make_shared<FakeBusControlBlock>();
    auto allocation = block->allocateSlot(nullptr);
    ASSERT_TRUE(allocation.id.valid());
    ASSERT_NE(allocation.state, nullptr);

    auto token = std::make_unique<ConnectionToken>(
        std::weak_ptr<IBusControlBlock>(block),
        allocation.id,
        allocation.state);

    // Pre-cancel: the bus is alive, the id is valid, and neither the
    // token's own _cancelled atomic nor the shared SlotState's
    // cancelled flag has been flipped — active() must say true.
    EXPECT_TRUE(token->active())
        << "fresh token over a live bus must report active() == true";
    EXPECT_TRUE(block->isAlive());

    // Cancel runs the full unregister-plus-barrier sequence. After
    // it returns, both the token's own _cancelled atomic and the
    // shared SlotState->cancelled flag are true; the bus itself is
    // still alive (cancel does not mark the bus dead).
    token->cancel();

    EXPECT_TRUE(block->isAlive())
        << "cancel() must not affect the bus's own alive flag";
    EXPECT_TRUE(allocation.state->cancelled)
        << "cancel() must trip the shared SlotState->cancelled flag";
    EXPECT_FALSE(token->active())
        << "active() must observe the explicit unregister and report false";
}

} // namespace
