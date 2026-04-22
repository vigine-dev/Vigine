#pragma once

#include <memory>

#include "vigine/pipelinebuilder/abstractpipelinebuilder.h"

namespace vigine::pipelinebuilder
{

/**
 * @brief Concrete final pipeline-builder facade.
 *
 * @ref DefaultPipelineBuilder is Level-5 of the five-layer wrapper
 * recipe.  It provides the full @ref IPipelineBuilder implementation on
 * top of @ref AbstractPipelineBuilder:
 *
 *   - A stage list accumulated by successive @ref addStage calls.
 *   - A single-call @ref build that wraps the stage list into a
 *     @ref DefaultPipeline, allocates a bounded drain channel via the
 *     injected @ref IChannelFactory, and returns the pipeline handle.
 *   - Guard logic that rejects @ref addStage and @ref build calls after
 *     the first successful @ref build.
 *
 * Callers obtain instances exclusively through @ref createPipelineBuilder.
 *
 * Thread-safety: @ref addStage and @ref build are NOT thread-safe with
 * respect to each other (builder use is typically single-threaded before
 * calling @ref build).  The produced @ref IPipeline's @ref IPipeline::feed
 * and @ref IPipeline::drain are thread-safe.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - INV-9:  @ref build returns @c std::unique_ptr<IPipeline>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultPipelineBuilder final : public AbstractPipelineBuilder
{
  public:
    /**
     * @brief Constructs the builder over @p bus, @p threadManager, and
     *        @p channelFactory.
     *
     * All three references must outlive this builder instance.
     */
    DefaultPipelineBuilder(vigine::messaging::IMessageBus       &bus,
                           vigine::threading::IThreadManager    &threadManager,
                           vigine::channelfactory::IChannelFactory &channelFactory);

    ~DefaultPipelineBuilder() override;

    // IPipelineBuilder
    IPipelineBuilder &addStage(std::unique_ptr<IPipelineStage> stage,
                               vigine::Result *outResult = nullptr) override;

    [[nodiscard]] std::unique_ptr<IPipeline>
        build(vigine::Result *outResult = nullptr) override;

    DefaultPipelineBuilder(const DefaultPipelineBuilder &)            = delete;
    DefaultPipelineBuilder &operator=(const DefaultPipelineBuilder &) = delete;
    DefaultPipelineBuilder(DefaultPipelineBuilder &&)                 = delete;
    DefaultPipelineBuilder &operator=(DefaultPipelineBuilder &&)      = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::pipelinebuilder
