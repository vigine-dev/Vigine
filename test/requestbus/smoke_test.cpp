#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/requestbus/defaultrequestbus.h"
#include "vigine/requestbus/factory.h"
#include "vigine/requestbus/ifuture.h"
#include "vigine/requestbus/irequestbus.h"
#include "vigine/requestbus/requestconfig.h"
#include "vigine/result.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/topicbus/topicid.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>

// ---------------------------------------------------------------------------
// Test suite: RequestBus smoke tests (label: requestbus-smoke)
//
// Scenario 1 — request/respond round-trip:
//   Build a bus + request facade. Register a responder via respondTo().
//   Issue a request. Responder calls respond() synchronously. Assert the
//   IFuture resolves and the reply payload is returned.
//
// Scenario 2 — request timeout returns nullopt:
//   Issue a request with a very short timeout (10 ms). No responder
//   replies. Assert IFuture::wait returns std::nullopt.
//
// Scenario 3 — late reply after TTL is dropped:
//   Issue a request with a custom short TTL. Wait until TTL elapses.
//   Then call respond() manually. Assert the future was already expired
//   (ready() true, but wait returns nullopt because state is Expired).
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine;
using namespace vigine::messaging;
using namespace vigine::requestbus;
using namespace vigine::topicbus;

static constexpr vigine::payload::PayloadTypeId kRequestTypeId{100};
static constexpr vigine::payload::PayloadTypeId kReplyTypeId{101};

// ---------------------------------------------------------------------------
// Minimal concrete IMessagePayload implementations.
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

// ---------------------------------------------------------------------------
// Fixture: thread manager + inline-only bus + request facade.
// ---------------------------------------------------------------------------

class RequestBusSmoke : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        _tm = vigine::core::threading::createThreadManager({});

        BusConfig cfg;
        cfg.threading    = ThreadingPolicy::InlineOnly;
        cfg.backpressure = BackpressurePolicy::Error;
        _bus = createMessageBus(cfg, *_tm);

        _rb = createRequestBus(*_bus, *_tm);
    }

    void TearDown() override
    {
        if (_rb)
        {
            _rb->shutdown();
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
    std::unique_ptr<IRequestBus>                       _rb;
};

// ---------------------------------------------------------------------------
// Scenario 1: request / respond round-trip
//
// A SynchronousResponder subscribes via respondTo(), receives the
// TopicRequest message in onMessage(), and immediately calls respond().
// Because the bus is InlineOnly, post() dispatches synchronously on the
// caller's thread -- the round-trip is entirely on-thread.
// ---------------------------------------------------------------------------

class SynchronousResponder final : public ISubscriber
{
  public:
    SynchronousResponder(IRequestBus *rb, vigine::payload::PayloadTypeId replyId)
        : _rb(rb), _replyId(replyId)
    {
    }

    [[nodiscard]] DispatchResult onMessage(const IMessage &msg) override
    {
        if (msg.kind() != MessageKind::TopicRequest)
        {
            return DispatchResult::Pass;
        }

        callCount.fetch_add(1, std::memory_order_relaxed);

        _rb->respond(msg.correlationId(),
                     std::make_unique<SmokePayload>(_replyId));

        return DispatchResult::Handled;
    }

    std::atomic<int> callCount{0};

  private:
    IRequestBus                       *_rb;
    vigine::payload::PayloadTypeId     _replyId;
};

TEST_F(RequestBusSmoke, RequestRespondRoundTrip)
{
    // Arrange: register a responder on topic 42.
    TopicId topic{42};
    SynchronousResponder responder{_rb.get(), kReplyTypeId};
    auto token = _rb->respondTo(topic, &responder);
    ASSERT_NE(token, nullptr) << "respondTo must return a non-null token";

    // Act: issue request with a generous timeout.
    RequestConfig cfg;
    cfg.timeout = std::chrono::milliseconds{500};

    auto future = _rb->request(topic, std::make_unique<SmokePayload>(kRequestTypeId), cfg);
    ASSERT_NE(future, nullptr) << "request must return a non-null IFuture";

    // Because the bus is InlineOnly, post() inside request() synchronously
    // calls responder.onMessage(), which calls respond(), which resolves
    // the future -- all before request() returns.
    EXPECT_TRUE(future->ready()) << "future should be resolved after synchronous dispatch";

    // Wait should return immediately with the reply payload.
    auto result = future->wait(std::chrono::milliseconds{50});
    ASSERT_TRUE(result.has_value()) << "wait must return a payload on resolved future";
    ASSERT_NE(*result, nullptr)     << "resolved payload must not be null";
    EXPECT_EQ((*result)->typeId(), kReplyTypeId)
        << "reply payload type id must match what the responder sent";

    EXPECT_EQ(responder.callCount.load(), 1)
        << "responder should have been called exactly once";
}

// ---------------------------------------------------------------------------
// Scenario 2: request timeout — no responder replies
// ---------------------------------------------------------------------------

TEST_F(RequestBusSmoke, RequestTimeoutReturnsNullopt)
{
    // Arrange: no responder registered on topic 99.
    TopicId topic{99};

    RequestConfig cfg;
    cfg.timeout = std::chrono::milliseconds{10};  // short timeout
    cfg.ttl     = std::chrono::milliseconds{20};  // explicit TTL

    auto future = _rb->request(topic, std::make_unique<SmokePayload>(kRequestTypeId), cfg);
    ASSERT_NE(future, nullptr) << "request must return a non-null IFuture even without responder";

    // Act: wait with the short timeout -- no responder will reply.
    auto result = future->wait(std::chrono::milliseconds{10});

    // Assert
    EXPECT_FALSE(result.has_value())
        << "wait must return nullopt when no reply arrives within timeout";
}

// ---------------------------------------------------------------------------
// Scenario 3: late reply after TTL is silently dropped
//
// We issue a request with a short TTL. After the TTL elapses, the
// FutureState transitions to Expired. We then call respond() manually --
// the FutureState tryResolve() returns false (already expired) so the
// future stays expired and wait() returns nullopt.
// ---------------------------------------------------------------------------

TEST_F(RequestBusSmoke, LateReplyAfterTtlDropped)
{
    // Arrange: capture the correlation id via a spy subscriber.
    TopicId topic{77};
    CorrelationId capturedCorrId{};

    class CapturingResponder final : public ISubscriber
    {
      public:
        CorrelationId *out{nullptr};
        [[nodiscard]] DispatchResult onMessage(const IMessage &msg) override
        {
            if (msg.kind() == MessageKind::TopicRequest && out)
            {
                *out = msg.correlationId();
            }
            return DispatchResult::Handled;
        }
    } spy;
    spy.out = &capturedCorrId;

    auto token = _rb->respondTo(topic, &spy);
    ASSERT_NE(token, nullptr);

    // Issue request with a very short TTL so it expires quickly.
    RequestConfig cfg;
    cfg.timeout = std::chrono::milliseconds{5000};   // caller will wait longer
    cfg.ttl     = std::chrono::milliseconds{30};     // very short TTL

    auto future = _rb->request(topic, std::make_unique<SmokePayload>(kRequestTypeId), cfg);
    ASSERT_NE(future, nullptr);
    ASSERT_TRUE(capturedCorrId.valid()) << "spy must have captured the correlation id";

    // Wait for the TTL cleanup task to expire the future.
    std::this_thread::sleep_for(std::chrono::milliseconds{80});

    // Assert: after TTL, future is in a terminal state (Expired).
    EXPECT_TRUE(future->ready())
        << "after TTL the future must be in a terminal state";

    // Now attempt a late reply -- should be silently dropped.
    _rb->respond(capturedCorrId, std::make_unique<SmokePayload>(kReplyTypeId));

    // Wait must still return nullopt because the state is Expired.
    auto result = future->wait(std::chrono::milliseconds{10});
    EXPECT_FALSE(result.has_value())
        << "wait must return nullopt for an expired future even after a late respond()";
}

} // namespace
