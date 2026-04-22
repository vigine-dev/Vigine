#pragma once

#include <memory>

#include "vigine/pipelinebuilder/ipipeline.h"
#include "vigine/pipelinebuilder/ipipelinestage.h"
#include "vigine/result.h"

namespace vigine::pipelinebuilder
{

/**
 * @brief Pure-virtual builder that composes a linear pipeline of stages.
 *
 * @ref IPipelineBuilder is the Level-2 facade for composable processing
 * pipelines (plan_22, R.3.3.2.4).  A caller:
 *   1. Obtains a builder from @ref createPipelineBuilder (factory.h).
 *   2. Calls @ref addStage one or more times to append stages in order.
 *   3. Calls @ref build once to produce a runnable @ref IPipeline.
 *   4. Feeds payloads through @ref IPipeline::feed and reads results
 *      from @ref IPipeline::drain.
 *
 * Builder-reuse policy:
 *   - Calling @ref build a second time on the same builder returns
 *     @c nullptr and sets @p outResult to
 *     @ref vigine::Result::Code::Error (AlreadyBuilt semantics).
 *   - Stages may not be added after @ref build has been called.
 *
 * V1 constraints:
 *   - Pipelines are linear (1→1 or 1→0 per stage).
 *   - Fan-out and DAG topologies are deferred to v2.
 *
 * Invariants:
 *   - INV-1:  no template parameters in the public surface.
 *   - INV-9:  @ref build returns @c std::unique_ptr<IPipeline>.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IPipelineBuilder
{
  public:
    virtual ~IPipelineBuilder() = default;

    /**
     * @brief Appends @p stage to the end of the stage chain.
     *
     * The builder takes unique ownership of the stage.  Stages are
     * executed in the order they are added.  Calling @ref addStage after
     * @ref build has been called is a no-op that returns @c *this and
     * sets @p outResult to an error @ref vigine::Result (if non-null).
     *
     * A null @p stage pointer is a programming error; implementations
     * ignore it and record the error in @p outResult if non-null.
     */
    virtual IPipelineBuilder &
        addStage(std::unique_ptr<IPipelineStage> stage,
                 vigine::Result *outResult = nullptr) = 0;

    /**
     * @brief Builds and returns the pipeline.
     *
     * The returned @ref IPipeline owns the drain channel and is
     * immediately ready to accept @ref IPipeline::feed calls.
     *
     * Returns @c nullptr when:
     *   - No stages have been added (empty pipeline).
     *   - @ref build was already called on this builder.
     *
     * When returning @c nullptr, @p outResult (if non-null) is set to an
     * error @ref vigine::Result describing the failure.
     *
     * On success, @p outResult (if non-null) is set to
     * @ref vigine::Result::Code::Success.
     */
    [[nodiscard]] virtual std::unique_ptr<IPipeline>
        build(vigine::Result *outResult = nullptr) = 0;

    IPipelineBuilder(const IPipelineBuilder &)            = delete;
    IPipelineBuilder &operator=(const IPipelineBuilder &) = delete;
    IPipelineBuilder(IPipelineBuilder &&)                 = delete;
    IPipelineBuilder &operator=(IPipelineBuilder &&)      = delete;

  protected:
    IPipelineBuilder() = default;
};

} // namespace vigine::pipelinebuilder
