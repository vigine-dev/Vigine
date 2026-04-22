// ---------------------------------------------------------------------------
// Scenario 3 -- messaging bus post + receive round-trip.
//
// The scope wants the contract suite to prove that the bus on the
// aggregator actually delivers a posted message to a subscriber whose
// filter matches. The shared context's system bus has a Dedicated
// threading policy which would force the test to sleep waiting for a
// worker tick; the scenario therefore uses a private InlineOnly stack
// from the fixture so assertion can run immediately after post().
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <memory>

namespace vigine::contract
{
namespace
{

using MessagingRoundTrip = EngineFixture;

TEST_F(MessagingRoundTrip, PostReachesMatchingSubscriber)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());
    auto &bus = stack.bus();

    CountingSubscriber subscriber{vigine::messaging::DispatchResult::Handled};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = vigine::payload::PayloadTypeId{0x10100u};

    auto token = bus.subscribe(filter, &subscriber);
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->active());

    const vigine::Result posted = bus.post(std::make_unique<ContractMessage>(
        vigine::messaging::MessageKind::Signal,
        vigine::messaging::RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{0x10100u}));
    EXPECT_TRUE(posted.isSuccess())
        << "post must succeed on a fresh InlineOnly bus; got: "
        << posted.message();

    EXPECT_EQ(subscriber.hits(), 1u)
        << "subscriber must receive exactly one dispatch";

    // Second post with no subscription for the new filter kind still
    // resolves without error; dispatch simply finds no match.
    const vigine::Result ghostPost = bus.post(std::make_unique<ContractMessage>(
        vigine::messaging::MessageKind::Event,
        vigine::messaging::RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{0x99999u}));
    EXPECT_TRUE(ghostPost.isSuccess());
    EXPECT_EQ(subscriber.hits(), 1u)
        << "Signal subscriber must not receive an Event kind post";
}

TEST_F(MessagingRoundTrip, ShutdownRejectsSubsequentPost)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());
    auto &bus = stack.bus();

    const vigine::Result first = bus.post(std::make_unique<ContractMessage>(
        vigine::messaging::MessageKind::Signal,
        vigine::messaging::RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{1}));
    EXPECT_TRUE(first.isSuccess());

    const vigine::Result shut = bus.shutdown();
    EXPECT_TRUE(shut.isSuccess());

    const vigine::Result after = bus.post(std::make_unique<ContractMessage>(
        vigine::messaging::MessageKind::Signal,
        vigine::messaging::RouteMode::FirstMatch,
        vigine::payload::PayloadTypeId{1}));
    EXPECT_TRUE(after.isError())
        << "post after shutdown must report an error Result";
}

} // namespace
} // namespace vigine::contract
