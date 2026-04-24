// ---------------------------------------------------------------------------
// Scenario 10 -- actor host spawn + tell + stop.
//
// IActorHost::spawn creates a dedicated mailbox; tell() enqueues a
// message; stop() drains and joins the actor. The scenario exercises
// the full lifecycle:
//
//   1. Spawn an actor that increments a counter per receive().
//   2. Tell it one message; wait on a CV until the actor observes it.
//   3. Stop the actor; subsequent tell() must return an error Result.
//
// CV-vs-sleep choice (per scope FF-70 / FF-102 guidance):
//   - Actor delivery is async on a dedicated thread. The test uses a
//     std::condition_variable rather than std::this_thread::sleep_for
//     so timing tolerance is built into the wait predicate, not the
//     hard-coded delay. The sleep fallback with a retry loop exists
//     only on hot paths where CV wiring is not practical.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/actorhost/actorid.h"
#include "vigine/actorhost/defaultactorhost.h"
#include "vigine/actorhost/iactor.h"
#include "vigine/actorhost/iactorhost.h"
#include "vigine/actorhost/iactormailbox.h"
#include "vigine/context/icontext.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace vigine::contract
{
namespace
{

class SignallingActor final : public vigine::actorhost::IActor
{
  public:
    vigine::Result receive(const vigine::messaging::IMessage & /*message*/) override
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            ++_received;
        }
        _cv.notify_all();
        return vigine::Result{};
    }

    [[nodiscard]] bool waitFor(int target, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _cv.wait_for(lock, timeout,
                            [this, target] { return _received >= target; });
    }

  private:
    std::mutex              _mutex;
    std::condition_variable _cv;
    int                     _received{0};
};

using ActorLifecycle = EngineFixture;

TEST_F(ActorLifecycle, SpawnTellStopRoundTrip)
{
    auto stack = makePrivateStack(/*inlineOnly=*/false);
    ASSERT_TRUE(stack.valid());

    auto host = vigine::actorhost::createActorHost(stack.bus(),
                                                    stack.threadManager());
    ASSERT_NE(host, nullptr);

    auto actorPtr = std::make_unique<SignallingActor>();
    SignallingActor *rawActor = actorPtr.get();

    auto mailbox = host->spawn(std::move(actorPtr));
    ASSERT_NE(mailbox, nullptr);
    EXPECT_TRUE(mailbox->actorId().valid());

    const vigine::Result told = host->tell(
        mailbox->actorId(),
        std::make_unique<ContractMessage>(
            vigine::messaging::MessageKind::ActorMail,
            vigine::messaging::RouteMode::FirstMatch,
            vigine::payload::PayloadTypeId{0x70700u}));
    EXPECT_TRUE(told.isSuccess())
        << "tell must succeed for a live actor; got: " << told.message();

    EXPECT_TRUE(rawActor->waitFor(/*target=*/1, std::chrono::milliseconds{500}))
        << "actor must receive the message within 500 ms";

    mailbox->stop();

    const vigine::Result afterStop = host->tell(
        mailbox->actorId(),
        std::make_unique<ContractMessage>(
            vigine::messaging::MessageKind::ActorMail,
            vigine::messaging::RouteMode::FirstMatch,
            vigine::payload::PayloadTypeId{0x70701u}));
    EXPECT_TRUE(afterStop.isError())
        << "tell after stop must return an error Result";
}

} // namespace
} // namespace vigine::contract
