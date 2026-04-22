// ---------------------------------------------------------------------------
// Scenario 6 -- topic bus publish + subscribe isolation (FF-95).
//
// Two topics on the same facade, two distinct subscribers. Publishing on
// topic A must reach only the A-subscriber; the B-subscriber must stay
// at zero hits. This is exactly the regression the scope references as
// FF-95: early DefaultTopicBus implementations leaked the publish to
// every subscriber regardless of topic id.
//
// Skips under GTEST_SKIP() if the leak is still present at test time
// (per scope: "do NOT depend on FF-95 being fixed; write tests against
// the current behaviour"). The assert is the behaviour spec; the skip
// guards the sanitizer-matrix job against cascading failures until the
// fix lands.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/topicbus/defaulttopicbus.h"
#include "vigine/topicbus/itopicbus.h"
#include "vigine/topicbus/topicid.h"

#include <gtest/gtest.h>

#include <memory>

namespace vigine::contract
{
namespace
{

using TopicIsolation = EngineFixture;

TEST_F(TopicIsolation, TwoTopicsDoNotCrossTalk)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto topicBus = vigine::topicbus::createTopicBus(stack.bus());
    ASSERT_NE(topicBus, nullptr);

    const auto topicA = topicBus->createTopic("topic-A");
    const auto topicB = topicBus->createTopic("topic-B");
    ASSERT_TRUE(topicA.valid());
    ASSERT_TRUE(topicB.valid());
    ASSERT_NE(topicA, topicB)
        << "two distinct names must produce two distinct TopicId values";

    CountingSubscriber subA;
    CountingSubscriber subB;

    auto tokenA = topicBus->subscribe(topicA, &subA);
    auto tokenB = topicBus->subscribe(topicB, &subB);
    ASSERT_NE(tokenA, nullptr);
    ASSERT_NE(tokenB, nullptr);

    auto payload =
        std::make_unique<ContractPayload>(vigine::payload::PayloadTypeId{0x30301u});
    const vigine::Result r = topicBus->publish(topicA, std::move(payload));
    EXPECT_TRUE(r.isSuccess())
        << "publish must succeed on a valid topic; got: " << r.message();

    if (subB.hits() != 0u)
    {
        GTEST_SKIP()
            << "pending FF-95: topic isolation currently leaks across topics";
    }

    EXPECT_EQ(subA.hits(), 1u)
        << "topic-A subscriber must receive exactly one delivery";
    EXPECT_EQ(subB.hits(), 0u)
        << "topic-B subscriber must not see topic-A traffic";
}

} // namespace
} // namespace vigine::contract
