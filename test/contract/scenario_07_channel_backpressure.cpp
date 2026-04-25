// ---------------------------------------------------------------------------
// Scenario 7 -- channel factory bounded-channel backpressure.
//
// A bounded channel of capacity 1 must accept one send, then reject a
// non-blocking trySend() and time out a blocking send() with a short
// timeout. After a receive drains the queue, the next send succeeds
// again -- which is the full backpressure round-trip.
//
// CV-vs-sleep choice:
//   - All primitives have a timed entry point (send(timeoutMs),
//     receive(timeoutMs)). No std::this_thread::sleep_for is needed;
//     the test's upper time bound is the explicit timeout on each call.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/factory.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/api/channelfactory/ichannelfactory.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace vigine::contract
{
namespace
{

using ChannelBackpressure = EngineFixture;

TEST_F(ChannelBackpressure, BoundedChannelEnforcesCapacity)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto factory = vigine::channelfactory::createChannelFactory(stack.bus());
    ASSERT_NE(factory, nullptr);

    const vigine::payload::PayloadTypeId typeId{0x40401u};

    vigine::Result createResult{};
    auto channel = factory->create(
        vigine::channelfactory::ChannelKind::Bounded,
        /*capacity=*/1,
        typeId,
        &createResult);
    ASSERT_NE(channel, nullptr);
    ASSERT_TRUE(createResult.isSuccess())
        << "bounded channel factory must accept capacity=1; got: "
        << createResult.message();

    // Fill the one slot.
    auto firstPayload = std::make_unique<ContractPayload>(typeId);
    const vigine::Result firstSend =
        channel->send(std::move(firstPayload), /*timeoutMs=*/50);
    EXPECT_TRUE(firstSend.isSuccess())
        << "first send must succeed; got: " << firstSend.message();
    EXPECT_EQ(channel->size(), 1u);

    // Non-blocking send on a full channel must fail without consuming
    // the payload.
    auto                              secondPayload = std::make_unique<ContractPayload>(typeId);
    std::unique_ptr<vigine::messaging::IMessagePayload> secondHolder =
        std::move(secondPayload);
    EXPECT_FALSE(channel->trySend(secondHolder))
        << "trySend on a full bounded channel must return false";
    EXPECT_NE(secondHolder, nullptr)
        << "failed trySend must leave the caller's pointer intact";

    // Blocking send with a small timeout must fail with an error Result.
    const vigine::Result timedSend =
        channel->send(std::move(secondHolder), /*timeoutMs=*/20);
    EXPECT_TRUE(timedSend.isError())
        << "send with capacity exhausted must time out";

    // Drain and verify the next send succeeds again.
    std::unique_ptr<vigine::messaging::IMessagePayload> drained;
    const vigine::Result received = channel->receive(drained, /*timeoutMs=*/50);
    EXPECT_TRUE(received.isSuccess());
    EXPECT_NE(drained, nullptr);
    EXPECT_EQ(channel->size(), 0u);

    auto thirdPayload = std::make_unique<ContractPayload>(typeId);
    const vigine::Result thirdSend =
        channel->send(std::move(thirdPayload), /*timeoutMs=*/50);
    EXPECT_TRUE(thirdSend.isSuccess());
}

} // namespace
} // namespace vigine::contract
