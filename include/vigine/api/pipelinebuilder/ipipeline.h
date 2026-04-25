#pragma once

#include <memory>

#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/result.h"

namespace vigine::pipelinebuilder
{

/**
 * @brief Pure-virtual handle to a built, runnable pipeline.
 *
 * @ref IPipeline is the run-time object produced by
 * @ref IPipelineBuilder::build.  A pipeline chains one or more
 * @ref IPipelineStage instances in sequence: each stage receives the
 * output of the previous stage and either transforms or drops it.  The
 * last stage's non-null output is pushed onto the drain channel
 * accessible via @ref drain.
 *
 * Lifecycle:
 *   - @ref feed may only be called after the builder has produced this
 *     object via @ref IPipelineBuilder::build.  Calling @ref feed before
 *     build is complete is not possible by construction (the handle does
 *     not exist yet).
 *   - @ref shutdown closes the drain channel and prevents further
 *     @ref feed calls from succeeding.  Idempotent.
 *
 * Ownership:
 *   - @ref feed takes unique ownership of @p payload on success.  On
 *     error, ownership remains with the caller.
 *   - @ref drain returns a reference to the channel owned by the
 *     pipeline; the caller borrows it.
 *
 * Thread-safety: @ref feed and @ref drain are safe to call concurrently.
 * @ref shutdown is safe to call from any thread; subsequent @ref feed
 * calls on other threads return @ref vigine::Result::Code::Error.
 *
 * Invariants:
 *   - INV-1:  no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IPipeline
{
  public:
    virtual ~IPipeline() = default;

    /**
     * @brief Feeds @p payload through the stage chain.
     *
     * The pipeline runs every stage in order on the calling thread.
     * If a stage returns @c nullptr, the item is dropped and
     * @ref vigine::Result::Code::Success is still returned (a drop is
     * not an error).  If all stages produce output, the result is pushed
     * onto the drain @ref vigine::channelfactory::IChannel.
     *
     * Returns an error @ref vigine::Result when:
     *   - The pipeline has been shut down.
     *   - The drain channel is full and has timed out (bounded channel).
     */
    [[nodiscard]] virtual vigine::Result
        feed(std::unique_ptr<vigine::messaging::IMessagePayload> payload) = 0;

    /**
     * @brief Returns a reference to the drain channel.
     *
     * Consumers read processed payloads from this channel.  The channel
     * is owned by the pipeline; the reference is valid for the pipeline's
     * lifetime.
     *
     * @ref IChannel::receive blocks until a processed payload arrives
     * or the channel is closed (shutdown).
     */
    [[nodiscard]] virtual vigine::channelfactory::IChannel &drain() noexcept = 0;

    /**
     * @brief Shuts the pipeline down.
     *
     * Closes the drain channel so any blocked @ref IChannel::receive
     * calls wake up.  Subsequent @ref feed calls return an error.
     * Idempotent.
     */
    virtual void shutdown() = 0;

    IPipeline(const IPipeline &)            = delete;
    IPipeline &operator=(const IPipeline &) = delete;
    IPipeline(IPipeline &&)                 = delete;
    IPipeline &operator=(IPipeline &&)      = delete;

  protected:
    IPipeline() = default;
};

} // namespace vigine::pipelinebuilder
