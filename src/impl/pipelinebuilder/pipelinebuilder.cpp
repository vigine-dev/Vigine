#include "vigine/impl/pipelinebuilder/pipelinebuilder.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/api/channelfactory/ichannelfactory.h"
#include "vigine/api/pipelinebuilder/factory.h"
#include "vigine/api/pipelinebuilder/ipipeline.h"
#include "vigine/api/pipelinebuilder/ipipelinestage.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::pipelinebuilder
{

// ---------------------------------------------------------------------------
// Well-known PayloadTypeId for the internal pipeline drain channel.
// Value 0xFFFF0001 sits in a private reserved range; production code that
// registers application types must stay below 0xFFFF0000 per the engine
// payload-range convention.  The drain channel is internal-only, so no
// registry entry is needed.
// ---------------------------------------------------------------------------

static constexpr vigine::payload::PayloadTypeId kDrainTypeId{0xFFFF0001u};

// ---------------------------------------------------------------------------
// PipelineOutputPayload — wraps any IMessagePayload produced by the last
// stage, advertising kDrainTypeId so it matches the drain channel's expected
// type id.  Ownership of the inner payload is transferred into this wrapper.
// The wrapper is strictly internal to this translation unit.
// ---------------------------------------------------------------------------

class PipelineOutputPayload final : public vigine::messaging::IMessagePayload
{
  public:
    explicit PipelineOutputPayload(
        std::unique_ptr<vigine::messaging::IMessagePayload> inner) noexcept
        : _inner(std::move(inner))
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kDrainTypeId;
    }

    /**
     * @brief Returns a mutable reference to the wrapped payload.
     *
     * Consumers that receive a @ref PipelineOutputPayload from the drain
     * channel can call @ref unwrap to retrieve the actual user payload.
     * In tests and application code, consumers downcast via
     * @c static_cast<PipelineOutputPayload*> after checking the typeId,
     * or rely on the fact that the drain yields their custom payloads
     * wrapped here.
     *
     * For smoke-test simplicity, unwrap returns the raw pointer.  The
     * wrapper retains ownership.
     */
    [[nodiscard]] vigine::messaging::IMessagePayload *inner() const noexcept
    {
        return _inner.get();
    }

    /**
     * @brief Releases ownership of the inner payload.
     *
     * Call this to transfer the inner payload out of the wrapper without
     * destroying it.  The wrapper is empty after this call.
     */
    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload> release() noexcept
    {
        return std::move(_inner);
    }

    PipelineOutputPayload(const PipelineOutputPayload &)            = delete;
    PipelineOutputPayload &operator=(const PipelineOutputPayload &) = delete;
    PipelineOutputPayload(PipelineOutputPayload &&)                 = delete;
    PipelineOutputPayload &operator=(PipelineOutputPayload &&)      = delete;

  private:
    std::unique_ptr<vigine::messaging::IMessagePayload> _inner;
};

// ---------------------------------------------------------------------------
// DefaultPipeline — the runnable pipeline produced by PipelineBuilder.
// ---------------------------------------------------------------------------

class DefaultPipeline final : public IPipeline
{
  public:
    DefaultPipeline(std::vector<std::unique_ptr<IPipelineStage>> stages,
                    std::unique_ptr<vigine::channelfactory::IChannel> drainChannel)
        : _stages(std::move(stages))
        , _drainChannel(std::move(drainChannel))
        , _shutdown(false)
    {
    }

    ~DefaultPipeline() override
    {
        shutdown();
    }

    // IPipeline
    [[nodiscard]] vigine::Result
        feed(std::unique_ptr<vigine::messaging::IMessagePayload> payload) override
    {
        if (_shutdown.load(std::memory_order_acquire))
        {
            return vigine::Result{vigine::Result::Code::Error, "pipeline is shut down"};
        }

        if (!payload)
        {
            return vigine::Result{vigine::Result::Code::Error, "null payload"};
        }

        // Run each stage in sequence.  A stage may return nullptr to drop.
        for (auto &stage : _stages)
        {
            payload = stage->process(std::move(payload));
            if (!payload)
            {
                // Item dropped — not an error.
                return vigine::Result{};
            }
        }

        // Wrap the surviving payload with kDrainTypeId before pushing onto
        // the drain channel.  The channel validates that the sent payload
        // advertises kDrainTypeId; the wrapper satisfies this invariant.
        auto wrapped = std::make_unique<PipelineOutputPayload>(std::move(payload));

        // Use blocking send with no timeout (wait indefinitely) so
        // back-pressure from a bounded drain channel is naturally applied.
        // Unbounded drain channels never block.
        auto result = _drainChannel->send(std::move(wrapped), -1);
        if (result.isError())
        {
            return vigine::Result{vigine::Result::Code::Error,
                                  "drain channel rejected payload: " + result.message()};
        }

        return vigine::Result{};
    }

    [[nodiscard]] vigine::channelfactory::IChannel &drain() noexcept override
    {
        return *_drainChannel;
    }

    void shutdown() override
    {
        bool already = _shutdown.exchange(true, std::memory_order_acq_rel);
        if (!already)
        {
            _drainChannel->close();
        }
    }

    DefaultPipeline(const DefaultPipeline &)            = delete;
    DefaultPipeline &operator=(const DefaultPipeline &) = delete;
    DefaultPipeline(DefaultPipeline &&)                 = delete;
    DefaultPipeline &operator=(DefaultPipeline &&)      = delete;

  private:
    std::vector<std::unique_ptr<IPipelineStage>>      _stages;
    std::unique_ptr<vigine::channelfactory::IChannel> _drainChannel;
    std::atomic<bool>                                 _shutdown;
};

// ---------------------------------------------------------------------------
// PipelineBuilder::Impl
// ---------------------------------------------------------------------------

struct PipelineBuilder::Impl
{
    std::vector<std::unique_ptr<IPipelineStage>> stages;
    bool                                         built{false};
};

// ---------------------------------------------------------------------------
// PipelineBuilder
// ---------------------------------------------------------------------------

PipelineBuilder::PipelineBuilder(
    vigine::messaging::IMessageBus       &bus,
    vigine::core::threading::IThreadManager    &threadManager,
    vigine::channelfactory::IChannelFactory &channelFactory)
    : AbstractPipelineBuilder(bus, threadManager, channelFactory)
    , _impl(std::make_unique<Impl>())
{
}

PipelineBuilder::~PipelineBuilder() = default;

IPipelineBuilder &PipelineBuilder::addStage(
    std::unique_ptr<IPipelineStage> stage,
    vigine::Result *outResult)
{
    if (_impl->built)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "builder already built; addStage ignored"};
        }
        return *this;
    }

    if (!stage)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "null stage pointer"};
        }
        return *this;
    }

    _impl->stages.push_back(std::move(stage));

    if (outResult)
    {
        *outResult = vigine::Result{};
    }

    return *this;
}

std::unique_ptr<IPipeline> PipelineBuilder::build(vigine::Result *outResult)
{
    if (_impl->built)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "build() called twice on the same builder"};
        }
        return nullptr;
    }

    if (_impl->stages.empty())
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "no stages added; pipeline would be empty"};
        }
        return nullptr;
    }

    // Create an Unbounded drain channel.  Unbounded channels require
    // capacity == 0.  The drain type id is the internal sentinel.
    vigine::Result createResult;
    auto drainChannel = channelFactory().create(
        vigine::channelfactory::ChannelKind::Unbounded,
        0,
        kDrainTypeId,
        &createResult);

    if (!drainChannel)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "failed to create drain channel: " +
                                        createResult.message()};
        }
        return nullptr;
    }

    _impl->built = true;

    if (outResult)
    {
        *outResult = vigine::Result{};
    }

    return std::make_unique<DefaultPipeline>(
        std::move(_impl->stages),
        std::move(drainChannel));
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IPipelineBuilder>
createPipelineBuilder(vigine::messaging::IMessageBus       &bus,
                      vigine::core::threading::IThreadManager    &threadManager,
                      vigine::channelfactory::IChannelFactory &channelFactory)
{
    return std::make_unique<PipelineBuilder>(bus, threadManager, channelFactory);
}

} // namespace vigine::pipelinebuilder
