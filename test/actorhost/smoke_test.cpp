#include "vigine/api/actorhost/actorid.h"
#include "vigine/api/actorhost/factory.h"
#include "vigine/api/actorhost/iactor.h"
#include "vigine/api/actorhost/iactorhost.h"
#include "vigine/api/actorhost/iactormailbox.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

// ---------------------------------------------------------------------------
// Test suite: ActorHost smoke tests (label: actorhost-smoke)
//
// Scenario 1 — spawn + tell round-trip:
//   Spawn an actor. Tell it a message. Assert IActor::receive was called
//   exactly once with the expected payload.
//
// Scenario 2 — stop halts delivery:
//   Spawn an actor. Stop it. Subsequent tell must return an error Result.
//
// Scenario 3 — shutdown drains mailboxes:
//   Spawn two actors. Enqueue several messages. Call shutdown. Assert all
//   actors were stopped and no messages are in flight after shutdown returns.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine::actorhost;

// ---------------------------------------------------------------------------
// Minimal IMessage test double carrying a typed payload.
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

class SmokeMessage final : public vigine::messaging::IMessage
{
  public:
    explicit SmokeMessage(vigine::payload::PayloadTypeId typeId)
        : _payload(std::make_unique<SmokePayload>(typeId))
        , _when(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::ActorMail;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        return _payload->typeId();
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FirstMatch;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point scheduledFor() const noexcept override
    {
        return _when;
    }

  private:
    std::unique_ptr<SmokePayload>              _payload;
    std::chrono::steady_clock::time_point       _when;
};

// ---------------------------------------------------------------------------
// ActorCounters — shared counters kept alive by both test and actor.
// ---------------------------------------------------------------------------

struct ActorCounters
{
    std::atomic<int> receiveCount{0};
    std::atomic<int> startCount{0};
    std::atomic<int> stopCount{0};
};

// ---------------------------------------------------------------------------
// CountingActor — records how many times receive() is called.
// The counters are owned by a shared_ptr so the test can read them safely
// after the actor instance has been destroyed.
// ---------------------------------------------------------------------------

class CountingActor final : public IActor
{
  public:
    explicit CountingActor(std::shared_ptr<ActorCounters> counters)
        : _counters(std::move(counters))
    {
    }

    vigine::Result onStart() override
    {
        _counters->startCount.fetch_add(1, std::memory_order_relaxed);
        return vigine::Result{};
    }

    vigine::Result receive(const vigine::messaging::IMessage & /*message*/) override
    {
        _counters->receiveCount.fetch_add(1, std::memory_order_relaxed);
        return vigine::Result{};
    }

    void onStop() override
    {
        _counters->stopCount.fetch_add(1, std::memory_order_relaxed);
    }

  private:
    std::shared_ptr<ActorCounters> _counters;
};

// ---------------------------------------------------------------------------
// CrashingActor — throws from receive(); used to verify crash isolation.
// ---------------------------------------------------------------------------

class CrashingActor final : public IActor
{
  public:
    explicit CrashingActor(std::shared_ptr<std::atomic<int>> callCount)
        : _callCount(std::move(callCount))
    {
    }

    vigine::Result receive(const vigine::messaging::IMessage & /*message*/) override
    {
        _callCount->fetch_add(1, std::memory_order_relaxed);
        throw std::runtime_error("actor crash");
    }

  private:
    std::shared_ptr<std::atomic<int>> _callCount;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ActorHostSmoke : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        _tm = vigine::core::threading::createThreadManager({});

        vigine::messaging::BusConfig cfg;
        cfg.threading    = vigine::messaging::ThreadingPolicy::InlineOnly;
        cfg.backpressure = vigine::messaging::BackpressurePolicy::Error;
        _bus  = vigine::messaging::createMessageBus(cfg, *_tm);

        _host = createActorHost(*_bus, *_tm);
    }

    void TearDown() override
    {
        if (_host)
        {
            _host->shutdown();
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
    std::unique_ptr<vigine::messaging::IMessageBus>    _bus;
    std::unique_ptr<IActorHost>                        _host;
};

// ---------------------------------------------------------------------------
// Scenario 1: spawn + tell round-trip
// ---------------------------------------------------------------------------

TEST_F(ActorHostSmoke, SpawnTellReceiveRoundTrip)
{
    auto counters = std::make_shared<ActorCounters>();
    auto mailbox  = _host->spawn(std::make_unique<CountingActor>(counters));
    ASSERT_NE(mailbox, nullptr) << "spawn must return a valid mailbox";
    EXPECT_TRUE(mailbox->actorId().valid());

    auto result = _host->tell(mailbox->actorId(),
                              std::make_unique<SmokeMessage>(
                                  vigine::payload::PayloadTypeId{1}));
    EXPECT_TRUE(result.isSuccess())
        << "tell must succeed for a live actor; got: " << result.message();

    // Allow the actor's thread to process the message.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(counters->receiveCount.load(), 1)
        << "actor must have received exactly one message";
    EXPECT_GE(counters->startCount.load(), 1)
        << "onStart must have been called";

    mailbox->stop();

    // stop() is synchronous; onStop must be complete by now.
    EXPECT_GE(counters->stopCount.load(), 1)
        << "onStop must have been called after stop()";
}

// ---------------------------------------------------------------------------
// Scenario 2: stop halts delivery
// ---------------------------------------------------------------------------

TEST_F(ActorHostSmoke, StopHaltsDelivery)
{
    auto mailbox = _host->spawn(
        std::make_unique<CountingActor>(std::make_shared<ActorCounters>()));
    ASSERT_NE(mailbox, nullptr);

    ActorId id = mailbox->actorId();

    mailbox->stop();

    // After stop, tell must return an error.
    auto result = _host->tell(id, std::make_unique<SmokeMessage>(
                                      vigine::payload::PayloadTypeId{2}));
    EXPECT_TRUE(result.isError())
        << "tell after stop must return an error result";
}

// ---------------------------------------------------------------------------
// Scenario 3: shutdown drains mailboxes
// ---------------------------------------------------------------------------

TEST_F(ActorHostSmoke, ShutdownDrainsMailboxes)
{
    // Spawn two actors with shared counters so we can read after shutdown.
    auto c1 = std::make_shared<ActorCounters>();
    auto c2 = std::make_shared<ActorCounters>();
    auto mb1 = _host->spawn(std::make_unique<CountingActor>(c1));
    auto mb2 = _host->spawn(std::make_unique<CountingActor>(c2));
    ASSERT_NE(mb1, nullptr);
    ASSERT_NE(mb2, nullptr);

    ActorId id1 = mb1->actorId();
    ActorId id2 = mb2->actorId();

    // Enqueue a batch for each actor.
    constexpr int kMessages = 5;
    for (int i = 0; i < kMessages; ++i)
    {
        (void)_host->tell(id1, std::make_unique<SmokeMessage>(
                              vigine::payload::PayloadTypeId{10}));
        (void)_host->tell(id2, std::make_unique<SmokeMessage>(
                              vigine::payload::PayloadTypeId{11}));
    }

    // Release mailbox handles before shutdown so shutdown owns cleanup.
    mb1.reset();
    mb2.reset();

    // shutdown() must drain all mailboxes and return synchronously.
    _host->shutdown();

    // All messages must have been delivered (shutdown drains then joins).
    EXPECT_EQ(c1->receiveCount.load(), kMessages)
        << "actor 1 must have received all messages before shutdown returns";
    EXPECT_EQ(c2->receiveCount.load(), kMessages)
        << "actor 2 must have received all messages before shutdown returns";
}

// ---------------------------------------------------------------------------
// Scenario 4: crash in receive is logged; actor continues receiving
// ---------------------------------------------------------------------------

TEST_F(ActorHostSmoke, CrashInReceiveContinues)
{
    auto callCount = std::make_shared<std::atomic<int>>(0);
    auto mailbox   = _host->spawn(std::make_unique<CrashingActor>(callCount));
    ASSERT_NE(mailbox, nullptr);

    // Send two messages; both should be attempted even though the first throws.
    (void)_host->tell(mailbox->actorId(),
                      std::make_unique<SmokeMessage>(vigine::payload::PayloadTypeId{20}));
    (void)_host->tell(mailbox->actorId(),
                      std::make_unique<SmokeMessage>(vigine::payload::PayloadTypeId{21}));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(callCount->load(), 2)
        << "both messages must be attempted even after a crash in receive";

    mailbox->stop();
}

} // namespace
