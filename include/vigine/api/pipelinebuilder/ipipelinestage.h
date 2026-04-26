#pragma once

#include <memory>

#include "vigine/api/messaging/imessagepayload.h"

namespace vigine::pipelinebuilder
{

/**
 * @brief Pure-virtual hook for a single pipeline stage.
 *
 * @ref IPipelineStage is the unit of work in the pipeline-builder facade
 * (plan_22, R.3.3.2.4).  Each stage receives one @ref IMessagePayload
 * and either produces a transformed payload or drops the item by
 * returning @c nullptr.
 *
 * Drop semantics:
 *   - Returning @c nullptr from @ref process drops the item.  No further
 *     stages in the chain are called for that item, and nothing is
 *     pushed to the drain channel.  This is not an error.
 *   - To propagate an error downstream, return a payload whose
 *     application-level status field signals the error so that later
 *     stages can decide how to handle it.
 *
 * Ownership:
 *   - The argument @p payload is borrowed for the duration of the call.
 *     The stage must not hold the pointer after @ref process returns.
 *   - The returned unique_ptr transfers ownership to the pipeline; on
 *     @c nullptr return, ownership is not transferred (item is dropped).
 *
 * Thread-safety: the pipeline calls @ref process from a single dedicated
 * worker thread; implementations do not need to be internally thread-safe
 * with respect to the same stage instance.
 *
 * Invariants:
 *   - INV-1:  no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IPipelineStage
{
  public:
    virtual ~IPipelineStage() = default;

    /**
     * @brief Transforms @p payload and returns the result, or @c nullptr
     *        to drop the item.
     *
     * Implementations perform their domain work on the payload bytes and
     * return either:
     *   - A new (or the same) @c unique_ptr<IMessagePayload> to pass to
     *     the next stage.
     *   - @c nullptr to signal that this item should be dropped silently.
     *
     * Throwing an exception propagates out of @ref IPipeline::feed.
     * V1 pipelines do not catch stage exceptions; callers that need
     * isolation should wrap the stage body in a try-catch.
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::messaging::IMessagePayload>
        process(std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    IPipelineStage(const IPipelineStage &)            = delete;
    IPipelineStage &operator=(const IPipelineStage &) = delete;
    IPipelineStage(IPipelineStage &&)                 = delete;
    IPipelineStage &operator=(IPipelineStage &&)      = delete;

  protected:
    IPipelineStage() = default;
};

} // namespace vigine::pipelinebuilder
