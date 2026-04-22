#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"
#include "vigine/topicbus/defaulttopicbus.h"
#include "vigine/topicbus/itopicbus.h"
#include "vigine/topicbus/topicid.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string_view>
#include <utility>

// ---------------------------------------------------------------------------
// Test suite: TopicBus smoke tests (label: topicbus-smoke)
//
// Scenario 1 — publish/subscribe round-trip:
//   Build a bus + topic facade. Create a topic. Subscribe. Publish. Assert
//   subscriber received exactly one delivery.
//
// Scenario 2 — createTopic returns a stable id:
//   Call createTopic("foo") twice and assert both calls return the same
//   TopicId with a non-zero value.
//
// Scenario 3 — topicByName returns nullopt for unknown name:
//   Call topicByName on a name that was never registered; assert nullopt.
//
// Scenario 4 — empty name is rejected:
//   createTopic("") must return an invalid TopicId (value == 0).
//
// Scenario 5 — publish to invalid TopicId returns error:
//   publish(TopicId{0}, ...) must return Result::Code::Error.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine;
using namespace vigine::messaging;
using namespace vigine::topicbus;

// ---------------------------------------------------------------------------
// Test doubles
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

class CountingSubscriber final : public ISubscriber
{
  public:
    std::atomic<int> callCount{0};

    [[nodiscard]] DispatchResult onMessage(const IMessage &) override
    {
        callCount.fetch_add(1, std::memory_order_relaxed);
        return DispatchResult::Handled;
    }
};

// ---------------------------------------------------------------------------
// Fixture: creates a thread manager + inline-only bus + topic facade.
// ---------------------------------------------------------------------------

class TopicBusSmoke : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        _tm = vigine::threading::createThreadManager({});

        BusConfig cfg;
        cfg.threading    = ThreadingPolicy::InlineOnly;
        cfg.backpressure = BackpressurePolicy::Error;
        _bus = createMessageBus(cfg, *_tm);

        _topic = createTopicBus(*_bus);
    }

    void TearDown() override
    {
        if (_topic)
        {
            _topic->shutdown();
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
    std::unique_ptr<IMessageBus>                       _bus;
    std::unique_ptr<ITopicBus>                         _topic;
};

// ---------------------------------------------------------------------------
// Scenario 1: publish/subscribe round-trip
// ---------------------------------------------------------------------------

TEST_F(TopicBusSmoke, PublishSubscribeRoundTrip)
{
    // Arrange
    TopicId tid = _topic->createTopic("events");
    ASSERT_TRUE(tid.valid()) << "expected a valid TopicId for 'events'";

    CountingSubscriber sub;
    auto token = _topic->subscribe(tid, &sub);
    ASSERT_NE(token, nullptr) << "subscribe must return a non-null token";
    EXPECT_TRUE(token->active());

    // Act
    auto payload = std::make_unique<SmokePayload>(vigine::payload::PayloadTypeId{42});
    auto result  = _topic->publish(tid, std::move(payload));

    // Assert
    EXPECT_TRUE(result.isSuccess())
        << "publish should succeed; got: " << result.message();
    EXPECT_EQ(sub.callCount.load(), 1)
        << "subscriber should have been called exactly once";
}

// ---------------------------------------------------------------------------
// Scenario 2: createTopic returns a stable id
// ---------------------------------------------------------------------------

TEST_F(TopicBusSmoke, CreateTopicReturnsStableId)
{
    TopicId first  = _topic->createTopic("stable");
    TopicId second = _topic->createTopic("stable");

    EXPECT_TRUE(first.valid())   << "first call must return a valid id";
    EXPECT_EQ(first, second)     << "repeated createTopic must return the same id";
}

// ---------------------------------------------------------------------------
// Scenario 3: topicByName returns nullopt for unknown name
// ---------------------------------------------------------------------------

TEST_F(TopicBusSmoke, TopicByNameUnknownReturnsNullopt)
{
    auto result = _topic->topicByName("never-registered");
    EXPECT_FALSE(result.has_value())
        << "topicByName must return nullopt for an unregistered name";
}

// ---------------------------------------------------------------------------
// Scenario 4: empty name is rejected
// ---------------------------------------------------------------------------

TEST_F(TopicBusSmoke, EmptyNameRejected)
{
    TopicId tid = _topic->createTopic("");
    EXPECT_FALSE(tid.valid())
        << "createTopic(\"\") must return an invalid TopicId";
}

// ---------------------------------------------------------------------------
// Scenario 5: publish to invalid TopicId returns error
// ---------------------------------------------------------------------------

TEST_F(TopicBusSmoke, PublishInvalidTopicReturnsError)
{
    auto payload = std::make_unique<SmokePayload>(vigine::payload::PayloadTypeId{1});
    auto result  = _topic->publish(TopicId{0}, std::move(payload));

    EXPECT_TRUE(result.isError())
        << "publishing to TopicId{0} must return an error result";
}

} // namespace
