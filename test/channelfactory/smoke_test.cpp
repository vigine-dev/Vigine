#include "vigine/channelfactory/channelkind.h"
#include "vigine/channelfactory/defaultchannelfactory.h"
#include "vigine/channelfactory/factory.h"
#include "vigine/channelfactory/ichannel.h"
#include "vigine/channelfactory/ichannelfactory.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <utility>

// ---------------------------------------------------------------------------
// Test suite: ChannelFactory smoke tests (label: channelfactory-smoke)
//
// Scenario 1 — send/receive blocking round-trip:
//   Build factory. Create a Bounded channel of capacity 4. Send one payload.
//   Receive it back. Assert the round-trip succeeds and the payload type id
//   is preserved.
//
// Scenario 2 — close wakes waiters with an error result:
//   Create a Bounded channel of capacity 1. Block a thread in receive() on
//   an empty channel. Call close() from the main thread. Assert the blocked
//   receive returns an error result (channel closed).
//
// Scenario 3 — invalid config is rejected:
//   a) Bounded + capacity 0 must return null channel + error result.
//   b) Unbounded + capacity != 0 must return null channel + error result.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine;
using namespace vigine::messaging;
using namespace vigine::channelfactory;

// ---------------------------------------------------------------------------
// Minimal concrete IMessagePayload for tests.
// ---------------------------------------------------------------------------

class SmokePayload final : public vigine::messaging::IMessagePayload
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

// ---------------------------------------------------------------------------
// Fixture: creates a thread manager + inline-only bus + channel factory.
// ---------------------------------------------------------------------------

class ChannelFactorySmoke : public ::testing::Test
{
  protected:
    static constexpr vigine::payload::PayloadTypeId kTypeId{42};

    void SetUp() override
    {
        _tm = vigine::core::threading::createThreadManager({});

        BusConfig cfg;
        cfg.threading    = ThreadingPolicy::InlineOnly;
        cfg.backpressure = BackpressurePolicy::Error;
        _bus = createMessageBus(cfg, *_tm);

        _factory = createChannelFactory(*_bus);
    }

    void TearDown() override
    {
        if (_factory)
        {
            _factory->shutdown();
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

    std::unique_ptr<vigine::core::threading::IThreadManager> _tm;
    std::unique_ptr<IMessageBus>                       _bus;
    std::unique_ptr<IChannelFactory>                   _factory;
};

// ---------------------------------------------------------------------------
// Scenario 1: send/receive blocking round-trip
// ---------------------------------------------------------------------------

TEST_F(ChannelFactorySmoke, SendReceiveRoundTrip)
{
    // Arrange
    vigine::Result createResult;
    auto ch = _factory->create(ChannelKind::Bounded, 4, kTypeId, &createResult);
    ASSERT_NE(ch, nullptr)      << "create must succeed for Bounded+capacity=4";
    ASSERT_TRUE(createResult.isSuccess()) << createResult.message();

    // Act: send one payload
    auto sendPayload = std::make_unique<SmokePayload>(kTypeId);
    auto sendResult  = ch->send(std::move(sendPayload), 1000 /*ms*/);
    ASSERT_TRUE(sendResult.isSuccess())
        << "send should succeed on non-full channel: " << sendResult.message();

    // Act: receive it back
    std::unique_ptr<vigine::messaging::IMessagePayload> received;
    auto recvResult = ch->receive(received, 1000 /*ms*/);

    // Assert
    ASSERT_TRUE(recvResult.isSuccess())
        << "receive should return the enqueued payload: " << recvResult.message();
    ASSERT_NE(received, nullptr) << "received payload must not be null";
    EXPECT_EQ(received->typeId(), kTypeId)
        << "received payload type id must match what was sent";
    EXPECT_EQ(ch->size(), 0u) << "channel must be empty after the round-trip";
}

// ---------------------------------------------------------------------------
// Scenario 2: close wakes blocked receive with error
// ---------------------------------------------------------------------------

TEST_F(ChannelFactorySmoke, CloseWakesBlockedReceiver)
{
    // Arrange: capacity=1 Bounded channel, empty so receive will block.
    auto ch = _factory->create(ChannelKind::Bounded, 1, kTypeId);
    ASSERT_NE(ch, nullptr) << "create must succeed";
    ASSERT_FALSE(ch->isClosed());

    vigine::Result receiverResult;
    std::unique_ptr<vigine::messaging::IMessagePayload> receiverOut;

    // Launch a thread that blocks in receive().
    std::thread receiver([&] {
        receiverResult = ch->receive(receiverOut, -1 /*indefinite*/);
    });

    // Give the receiver thread a moment to enter the wait.
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    // Act: close the channel from the main thread.
    ch->close();
    receiver.join();

    // Assert: the blocked receive must have returned an error.
    EXPECT_TRUE(receiverResult.isError())
        << "receive on a closed-empty channel must return error; got: "
        << receiverResult.message();
    EXPECT_EQ(receiverOut, nullptr)
        << "out payload must remain null on closed-channel error";
    EXPECT_TRUE(ch->isClosed());
}

// ---------------------------------------------------------------------------
// Scenario 3a: Bounded + capacity 0 is rejected
// ---------------------------------------------------------------------------

TEST_F(ChannelFactorySmoke, BoundedZeroCapacityRejected)
{
    vigine::Result result;
    auto ch = _factory->create(ChannelKind::Bounded, 0, kTypeId, &result);

    EXPECT_EQ(ch, nullptr) << "Bounded+capacity=0 must return null channel";
    EXPECT_TRUE(result.isError())
        << "Bounded+capacity=0 must set error result; got: " << result.message();
}

// ---------------------------------------------------------------------------
// Scenario 3b: Unbounded + non-zero capacity is rejected
// ---------------------------------------------------------------------------

TEST_F(ChannelFactorySmoke, UnboundedNonZeroCapacityRejected)
{
    vigine::Result result;
    auto ch = _factory->create(ChannelKind::Unbounded, 5, kTypeId, &result);

    EXPECT_EQ(ch, nullptr) << "Unbounded+capacity=5 must return null channel";
    EXPECT_TRUE(result.isError())
        << "Unbounded+capacity!=0 must set error result; got: " << result.message();
}

} // namespace
