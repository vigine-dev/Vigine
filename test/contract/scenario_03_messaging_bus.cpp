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

#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/routemode.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
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

// Pins the contract that a SubscriptionToken can safely outlive the bus
// that produced it. The token holds a std::weak_ptr to the bus control
// block (not a raw pointer to the bus itself), so when the bus is
// destroyed first the token's cancel() path observes the locked weak_ptr
// returning null and becomes a no-op instead of chasing a dangling bus
// pointer into freed memory.
//
// The previous shape of the token stored the bus by raw pointer and
// called bus->removeSubscription(serial) unconditionally from its
// destructor; if the bus had already been freed (which is easy to
// arrange when an emitter in the application layer is declared after
// the engine that owns the bus), the call landed on freed memory and
// locked on a stale registry mutex -- the close-window hang that
// motivated this rework.
TEST_F(MessagingRoundTrip, TokenOutlivesBusWithoutHangOrCrash)
{
    std::unique_ptr<vigine::messaging::ISubscriptionToken> token;

    {
        auto stack = makePrivateStack(/*inlineOnly=*/true);
        ASSERT_TRUE(stack.valid());

        CountingSubscriber subscriber{
            vigine::messaging::DispatchResult::Handled};

        vigine::messaging::MessageFilter filter{};
        filter.kind   = vigine::messaging::MessageKind::Signal;
        filter.typeId = vigine::payload::PayloadTypeId{0x10102u};

        token = stack.bus().subscribe(filter, &subscriber);
        ASSERT_NE(token, nullptr);
        EXPECT_TRUE(token->active());

        // Inner scope exit first destroys the `subscriber` local, then
        // destroys `stack` (and with it the bus, then the thread
        // manager). `token` is held by the outer scope and survives.
        // The bus's shared_ptr to the control block drops with the
        // bus; since tokens hold only a weak_ptr, the control block
        // destructs alongside the bus and the weak_ptr lock after this
        // point returns null.
    }

    // The bus and its control block are gone. active() must report
    // false (the weak_ptr lock returns null), and dropping the token
    // must not chase a freed pointer.
    EXPECT_FALSE(token->active())
        << "token must report inactive once the bus is destroyed";

    // The destructor running here is the original bug site: previously
    // it called bus->removeSubscription() on a dangling pointer. Under
    // the new design, cancel() locks the weak_ptr, sees it is empty,
    // and returns without touching anything. Reaching the next line
    // without a crash or a hang is the pass condition.
    token.reset();
    SUCCEED() << "SubscriptionToken destructor survived bus destruction";
}

// Pins the contract that a token whose bus has been shut down (but not
// yet destroyed) also cancels cleanly. shutdown() calls markDead on the
// control block, and active() observes that through the
// IBusControlBlock::isAlive() check; the cancel path also short-
// circuits on the same check, so no unregister call reaches the
// already-drained registry.
TEST_F(MessagingRoundTrip, TokenCancelAfterShutdownIsNoOp)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());
    auto &bus = stack.bus();

    CountingSubscriber subscriber{vigine::messaging::DispatchResult::Handled};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = vigine::payload::PayloadTypeId{0x10103u};

    auto token = bus.subscribe(filter, &subscriber);
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->active());

    const vigine::Result shut = bus.shutdown();
    EXPECT_TRUE(shut.isSuccess());

    // After shutdown, active() must report false because the control
    // block's alive flag is down.
    EXPECT_FALSE(token->active())
        << "token must report inactive after the bus is shut down";

    // Dropping the token here runs cancel(); the cancel must take the
    // dead-path (no unregister call) without touching the bus internal
    // state. Reaching the line after the reset is the pass condition.
    token.reset();
    SUCCEED()
        << "SubscriptionToken destructor survived bus shutdown before bus death";
}

} // namespace
} // namespace vigine::contract
