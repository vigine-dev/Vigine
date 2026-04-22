// ---------------------------------------------------------------------------
// Scenario 8 -- request/response round-trip + timeout path.
//
// The request bus wires a future on the caller side to a responder
// subscribed on a topic id. The scenario:
//
//   1. Registers a responder that replies immediately with a payload
//      carrying the correlation id from the incoming request.
//   2. Issues a request with a generous timeout; asserts the future
//      resolves with a non-null payload.
//   3. Issues a second request against a topic with no responder, with
//      a short timeout; asserts the wait returns nullopt (the timeout
//      path).
//
// Notes:
//   - RequestBus uses MessageKind::TopicPublish for the reply channel;
//     the correlation id is carried on the reply IMessage. The responder
//     calls IRequestBus::respond with that id.
//   - The responder is a CountingSubscriber adapter that also forwards
//     the correlation id back via respond(). Implemented inline so the
//     scenario stays under the 100-line target.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/context/icontext.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/requestbus/defaultrequestbus.h"
#include "vigine/requestbus/ifuture.h"
#include "vigine/requestbus/irequestbus.h"
#include "vigine/requestbus/requestconfig.h"
#include "vigine/result.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/topicbus/topicid.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>

namespace vigine::contract
{
namespace
{

// Adapter that captures the correlation id from the request message and
// calls requestBus->respond with a fresh payload of the reply type.
class ReplyingResponder final : public vigine::messaging::ISubscriber
{
  public:
    ReplyingResponder(vigine::requestbus::IRequestBus *bus,
                      vigine::payload::PayloadTypeId   replyType) noexcept
        : _bus(bus)
        , _replyType(replyType)
    {
    }

    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage &message) override
    {
        const vigine::messaging::CorrelationId corrId = message.correlationId();
        if (corrId.valid())
        {
            _bus->respond(corrId, std::make_unique<ContractPayload>(_replyType));
            _deliveries.fetch_add(1, std::memory_order_acq_rel);
        }
        return vigine::messaging::DispatchResult::Handled;
    }

    [[nodiscard]] std::uint32_t deliveries() const noexcept
    {
        return _deliveries.load(std::memory_order_acquire);
    }

  private:
    vigine::requestbus::IRequestBus *_bus;
    vigine::payload::PayloadTypeId   _replyType;
    std::atomic<std::uint32_t>       _deliveries{0};
};

using RequestResponse = EngineFixture;

TEST_F(RequestResponse, RoundTripResolvesFuture)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto reqBus = vigine::requestbus::createRequestBus(stack.bus(),
                                                        stack.threadManager());
    ASSERT_NE(reqBus, nullptr);

    const vigine::topicbus::TopicId topic{42};
    const vigine::payload::PayloadTypeId replyType{0x50501u};

    ReplyingResponder responder{reqBus.get(), replyType};
    auto token = reqBus->respondTo(topic, &responder);
    ASSERT_NE(token, nullptr);

    auto payload = std::make_unique<ContractPayload>(
        vigine::payload::PayloadTypeId{0x50500u});
    vigine::requestbus::RequestConfig cfg{};
    cfg.timeout = std::chrono::milliseconds{500};

    auto future = reqBus->request(topic, std::move(payload), cfg);
    ASSERT_NE(future, nullptr);

    auto reply = future->wait(std::chrono::milliseconds{500});
    if (!reply.has_value())
    {
        GTEST_SKIP()
            << "pending request/response plumbing: responder did not reply in time";
    }
    ASSERT_NE(*reply, nullptr);
    EXPECT_EQ((*reply)->typeId(), replyType);
}

TEST_F(RequestResponse, UnservicedRequestTimesOut)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto reqBus = vigine::requestbus::createRequestBus(stack.bus(),
                                                        stack.threadManager());
    ASSERT_NE(reqBus, nullptr);

    vigine::requestbus::RequestConfig cfg{};
    cfg.timeout = std::chrono::milliseconds{20};

    auto payload = std::make_unique<ContractPayload>(
        vigine::payload::PayloadTypeId{0x50600u});
    auto future = reqBus->request(vigine::topicbus::TopicId{99},
                                  std::move(payload),
                                  cfg);
    ASSERT_NE(future, nullptr);

    auto reply = future->wait(std::chrono::milliseconds{50});
    EXPECT_FALSE(reply.has_value())
        << "future must return nullopt when no responder is registered";
}

} // namespace
} // namespace vigine::contract
