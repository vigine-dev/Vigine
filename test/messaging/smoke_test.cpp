#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/messaging/systemmessagebus.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
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
        _threadManager = vigine::threading::createThreadManager(
            vigine::threading::ThreadManagerConfig{});
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

    std::unique_ptr<vigine::threading::IThreadManager> _threadManager;
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

} // namespace
