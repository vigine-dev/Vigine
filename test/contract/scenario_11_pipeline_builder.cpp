// ---------------------------------------------------------------------------
// Scenario 11 -- pipeline builder two-stage.
//
// v1 pipelines are linear (1->1 per stage); fan-out is a v2 feature,
// not available today. The scenario therefore exercises the two-stage
// path which is the canonical shape:
//
//   stage 1: passthrough -- returns the same payload unchanged.
//   stage 2: tag counter  -- bumps a shared counter per item and
//                            returns the payload.
//
// The built pipeline accepts a single payload via feed(); the drain
// channel returns exactly one item.
// ---------------------------------------------------------------------------

#include "fixtures/contract_helpers.h"
#include "fixtures/engine_fixture.h"

#include "vigine/channelfactory/defaultchannelfactory.h"
#include "vigine/channelfactory/ichannel.h"
#include "vigine/channelfactory/ichannelfactory.h"
#include "vigine/api/context/icontext.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/pipelinebuilder/factory.h"
#include "vigine/pipelinebuilder/ipipeline.h"
#include "vigine/pipelinebuilder/ipipelinebuilder.h"
#include "vigine/pipelinebuilder/ipipelinestage.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <utility>

namespace vigine::contract
{
namespace
{

class PassThroughStage final : public vigine::pipelinebuilder::IPipelineStage
{
  public:
    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload>
        process(std::unique_ptr<vigine::messaging::IMessagePayload> payload) override
    {
        return payload;
    }
};

class CountingStage final : public vigine::pipelinebuilder::IPipelineStage
{
  public:
    explicit CountingStage(std::shared_ptr<std::atomic<int>> counter) noexcept
        : _counter(std::move(counter))
    {
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload>
        process(std::unique_ptr<vigine::messaging::IMessagePayload> payload) override
    {
        _counter->fetch_add(1, std::memory_order_acq_rel);
        return payload;
    }

  private:
    std::shared_ptr<std::atomic<int>> _counter;
};

using PipelineBuilder = EngineFixture;

TEST_F(PipelineBuilder, TwoStagePipelineDelivers)
{
    auto stack = makePrivateStack(/*inlineOnly=*/true);
    ASSERT_TRUE(stack.valid());

    auto channelFactory =
        vigine::channelfactory::createChannelFactory(stack.bus());
    ASSERT_NE(channelFactory, nullptr);

    auto builder = vigine::pipelinebuilder::createPipelineBuilder(
        stack.bus(), stack.threadManager(), *channelFactory);
    ASSERT_NE(builder, nullptr);

    auto counter = std::make_shared<std::atomic<int>>(0);
    builder->addStage(std::make_unique<PassThroughStage>());
    builder->addStage(std::make_unique<CountingStage>(counter));

    vigine::Result buildResult{};
    auto pipeline = builder->build(&buildResult);
    ASSERT_NE(pipeline, nullptr);
    ASSERT_TRUE(buildResult.isSuccess())
        << "build with two valid stages must succeed; got: "
        << buildResult.message();

    const vigine::payload::PayloadTypeId typeId{0x80801u};
    auto payload = std::make_unique<ContractPayload>(typeId);

    const vigine::Result fed = pipeline->feed(std::move(payload));
    EXPECT_TRUE(fed.isSuccess())
        << "feed on a fresh pipeline must succeed; got: " << fed.message();
    EXPECT_EQ(counter->load(), 1)
        << "the counting stage must see exactly one item";

    // The drain wraps each emitted payload in a well-known
    // PipelineOutputPayload with a reserved drain-channel type id
    // (0xFFFF0001). The contract here asserts the wrapper arrives; the
    // inner payload is inspected by consumer code that pays for its
    // own static_cast path. Since PipelineOutputPayload is private to
    // the translation unit shipping the implementation, the contract
    // suite cannot downcast; verifying the drain typeId is the
    // observable test.
    std::unique_ptr<vigine::messaging::IMessagePayload> drained;
    const vigine::Result received =
        pipeline->drain().receive(drained, /*timeoutMs=*/50);
    EXPECT_TRUE(received.isSuccess());
    EXPECT_NE(drained, nullptr);
    EXPECT_NE(drained->typeId(), vigine::payload::PayloadTypeId{})
        << "drain must emit a payload with a non-zero type id";

    pipeline->shutdown();
}

} // namespace
} // namespace vigine::contract
