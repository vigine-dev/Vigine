#include "vigine/channelfactory/channelkind.h"
#include "vigine/channelfactory/factory.h"
#include "vigine/channelfactory/ichannel.h"
#include "vigine/channelfactory/ichannelfactory.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/pipelinebuilder/defaultpipelinebuilder.h"
#include "vigine/pipelinebuilder/factory.h"
#include "vigine/pipelinebuilder/ipipeline.h"
#include "vigine/pipelinebuilder/ipipelinebuilder.h"
#include "vigine/pipelinebuilder/ipipelinestage.h"
#include "vigine/result.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Test suite: PipelineBuilder smoke tests (label: pipelinebuilder-smoke)
//
// Scenario 1 — two-stage linear pipeline round-trip:
//   Build a pipeline with two counting stages. Feed one payload. Assert both
//   stages were called and output arrives in the drain channel.
//
// Scenario 2 — stage returning nullptr drops the item:
//   Build a pipeline whose second stage returns nullptr. Feed one payload.
//   Assert drain receives nothing and feed() returns success (drop != error).
//
// Scenario 3 — build() twice returns nullptr (AlreadyBuilt):
//   Call build() once successfully, then call build() again. Assert the
//   second call returns nullptr with an error result.
//
// Scenario 4 — shutdown closes drain; subsequent feed returns error:
//   Build a pipeline. Shut it down. Feed a payload. Assert feed returns error.
//   drain() receives closed-channel error.
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine::pipelinebuilder;

// ---------------------------------------------------------------------------
// Minimal concrete IMessagePayload for test payloads.
// ---------------------------------------------------------------------------

class SmokePayload final : public vigine::messaging::IMessagePayload
{
  public:
    explicit SmokePayload(vigine::payload::PayloadTypeId id,
                          int                            tag = 0) noexcept
        : _id(id)
        , _tag(tag)
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _id;
    }

    [[nodiscard]] int tag() const noexcept
    {
        return _tag;
    }

  private:
    vigine::payload::PayloadTypeId _id;
    int                            _tag;
};

// ---------------------------------------------------------------------------
// CountingStage — passes payload through unchanged and increments a counter.
// ---------------------------------------------------------------------------

class CountingStage final : public IPipelineStage
{
  public:
    explicit CountingStage(std::shared_ptr<std::atomic<int>> counter)
        : _counter(std::move(counter))
    {
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload>
        process(std::unique_ptr<vigine::messaging::IMessagePayload> payload) override
    {
        _counter->fetch_add(1, std::memory_order_relaxed);
        return payload; // pass through
    }

  private:
    std::shared_ptr<std::atomic<int>> _counter;
};

// ---------------------------------------------------------------------------
// DroppingStage — always returns nullptr (drops the item).
// ---------------------------------------------------------------------------

class DroppingStage final : public IPipelineStage
{
  public:
    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload>
        process(std::unique_ptr<vigine::messaging::IMessagePayload> /*payload*/) override
    {
        return nullptr; // drop
    }
};

// ---------------------------------------------------------------------------
// Fixture: creates the infrastructure shared by all pipeline tests.
// ---------------------------------------------------------------------------

class PipelineBuilderSmoke : public ::testing::Test
{
  protected:
    static constexpr vigine::payload::PayloadTypeId kPayloadTypeId{100};

    void SetUp() override
    {
        _tm = vigine::core::threading::createThreadManager({});

        vigine::messaging::BusConfig cfg;
        cfg.threading    = vigine::messaging::ThreadingPolicy::InlineOnly;
        cfg.backpressure = vigine::messaging::BackpressurePolicy::Error;
        _bus = vigine::messaging::createMessageBus(cfg, *_tm);

        _channelFactory = vigine::channelfactory::createChannelFactory(*_bus);
    }

    void TearDown() override
    {
        if (_channelFactory)
        {
            _channelFactory->shutdown();
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

    /**
     * @brief Convenience builder factory.
     */
    [[nodiscard]] std::unique_ptr<IPipelineBuilder> makeBuilder() const
    {
        return createPipelineBuilder(*_bus, *_tm, *_channelFactory);
    }

    std::unique_ptr<vigine::core::threading::IThreadManager>     _tm;
    std::unique_ptr<vigine::messaging::IMessageBus>        _bus;
    std::unique_ptr<vigine::channelfactory::IChannelFactory> _channelFactory;
};

// ---------------------------------------------------------------------------
// Scenario 1: two-stage linear pipeline round-trip
//
// Asserts:
//   - Both stages are called exactly once per feed.
//   - The drain channel yields one payload after a single feed.
//   - feed() returns success.
// ---------------------------------------------------------------------------

TEST_F(PipelineBuilderSmoke, TwoStagePipelineRoundTrip)
{
    auto c1 = std::make_shared<std::atomic<int>>(0);
    auto c2 = std::make_shared<std::atomic<int>>(0);

    auto builder = makeBuilder();
    ASSERT_NE(builder, nullptr);

    vigine::Result addResult;
    builder->addStage(std::make_unique<CountingStage>(c1), &addResult);
    ASSERT_TRUE(addResult.isSuccess()) << addResult.message();

    builder->addStage(std::make_unique<CountingStage>(c2), &addResult);
    ASSERT_TRUE(addResult.isSuccess()) << addResult.message();

    vigine::Result buildResult;
    auto pipeline = builder->build(&buildResult);
    ASSERT_NE(pipeline, nullptr)       << "build must succeed for a non-empty builder";
    ASSERT_TRUE(buildResult.isSuccess()) << buildResult.message();

    // Feed one payload through the pipeline.
    auto feedResult = pipeline->feed(
        std::make_unique<SmokePayload>(kPayloadTypeId, 1));
    EXPECT_TRUE(feedResult.isSuccess())
        << "feed must succeed for a two-stage pipeline: " << feedResult.message();

    // Both stages must have been called.
    EXPECT_EQ(c1->load(), 1) << "stage 1 must be called exactly once";
    EXPECT_EQ(c2->load(), 1) << "stage 2 must be called exactly once";

    // Drain channel must contain exactly one payload.
    std::unique_ptr<vigine::messaging::IMessagePayload> received;
    auto recvResult = pipeline->drain().receive(received, 500 /*ms*/);
    EXPECT_TRUE(recvResult.isSuccess())
        << "drain must yield the processed payload: " << recvResult.message();
    EXPECT_NE(received, nullptr) << "received payload must not be null";

    pipeline->shutdown();
}

// ---------------------------------------------------------------------------
// Scenario 2: stage returning nullptr drops the item (not an error)
//
// Asserts:
//   - feed() returns success even when an item is dropped mid-chain.
//   - The drain channel remains empty (tryReceive returns false).
//   - The first stage is called; no stage after the dropping stage is called.
// ---------------------------------------------------------------------------

TEST_F(PipelineBuilderSmoke, DroppingStageItemNotPushedToDrain)
{
    auto c1 = std::make_shared<std::atomic<int>>(0);
    // Stage order: CountingStage -> DroppingStage -> CountingStage
    // The second CountingStage must NOT be called because DroppingStage drops.
    auto c3 = std::make_shared<std::atomic<int>>(0);

    auto builder = makeBuilder();
    builder->addStage(std::make_unique<CountingStage>(c1));
    builder->addStage(std::make_unique<DroppingStage>());
    builder->addStage(std::make_unique<CountingStage>(c3));

    auto pipeline = builder->build();
    ASSERT_NE(pipeline, nullptr);

    auto feedResult = pipeline->feed(
        std::make_unique<SmokePayload>(kPayloadTypeId, 2));
    EXPECT_TRUE(feedResult.isSuccess())
        << "drop is not an error; feed must return success: " << feedResult.message();

    EXPECT_EQ(c1->load(), 1) << "stage before dropping stage must be called";
    EXPECT_EQ(c3->load(), 0) << "stage after dropping stage must NOT be called";

    // Drain must be empty — nothing arrived after the drop.
    std::unique_ptr<vigine::messaging::IMessagePayload> out;
    bool hasItem = pipeline->drain().tryReceive(out);
    EXPECT_FALSE(hasItem) << "drain must be empty after a stage drops the item";
    EXPECT_EQ(out, nullptr);

    pipeline->shutdown();
}

// ---------------------------------------------------------------------------
// Scenario 3: build() called twice returns nullptr (AlreadyBuilt semantics)
//
// Asserts:
//   - First build() returns a valid pipeline.
//   - Second build() returns nullptr with an error result.
//   - The first pipeline is still usable after the second build() attempt.
// ---------------------------------------------------------------------------

TEST_F(PipelineBuilderSmoke, BuildTwiceReturnsError)
{
    auto builder = makeBuilder();
    builder->addStage(
        std::make_unique<CountingStage>(std::make_shared<std::atomic<int>>(0)));

    vigine::Result r1;
    auto pipeline = builder->build(&r1);
    ASSERT_NE(pipeline, nullptr)   << "first build must succeed";
    ASSERT_TRUE(r1.isSuccess())    << r1.message();

    // Second build on the same builder.
    vigine::Result r2;
    auto pipeline2 = builder->build(&r2);
    EXPECT_EQ(pipeline2, nullptr)  << "second build must return nullptr";
    EXPECT_TRUE(r2.isError())      << "second build must report an error result";

    // First pipeline is still operational.
    auto feedResult = pipeline->feed(
        std::make_unique<SmokePayload>(kPayloadTypeId, 3));
    EXPECT_TRUE(feedResult.isSuccess())
        << "first pipeline must still accept feeds: " << feedResult.message();

    pipeline->shutdown();
}

// ---------------------------------------------------------------------------
// Scenario 4: shutdown closes drain; subsequent feed returns error
//
// Asserts:
//   - After shutdown(), feed() returns an error result.
//   - drain().receive() returns an error when the channel is closed+empty.
// ---------------------------------------------------------------------------

TEST_F(PipelineBuilderSmoke, ShutdownRejectsFeed)
{
    auto builder = makeBuilder();
    builder->addStage(
        std::make_unique<CountingStage>(std::make_shared<std::atomic<int>>(0)));

    auto pipeline = builder->build();
    ASSERT_NE(pipeline, nullptr);

    pipeline->shutdown();

    // Post-shutdown feed must fail.
    auto feedResult = pipeline->feed(
        std::make_unique<SmokePayload>(kPayloadTypeId, 4));
    EXPECT_TRUE(feedResult.isError())
        << "feed after shutdown must return error: " << feedResult.message();

    // drain().receive() on a closed channel must also fail.
    std::unique_ptr<vigine::messaging::IMessagePayload> out;
    auto recvResult = pipeline->drain().receive(out, 0 /*ms*/);
    EXPECT_TRUE(recvResult.isError())
        << "receive on closed+empty drain must return error";
}

} // namespace
