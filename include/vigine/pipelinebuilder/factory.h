#pragma once

#include <memory>

#include "vigine/channelfactory/ichannelfactory.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/pipelinebuilder/defaultpipelinebuilder.h"
#include "vigine/threading/ithreadmanager.h"

// factory.h is a convenience header that re-exports createPipelineBuilder
// so callers can include a single predictable factory header rather than
// naming the concrete DefaultPipelineBuilder type.  The function is
// defined in src/pipelinebuilder/defaultpipelinebuilder.cpp.
//
// Invariants:
//   - INV-9:  createPipelineBuilder returns std::unique_ptr<IPipelineBuilder>.
//   - INV-11: no graph types appear here.

namespace vigine::pipelinebuilder
{

/**
 * @brief Factory function — the sole entry point for creating a
 *        pipeline-builder facade.
 *
 * Returns a @c std::unique_ptr<IPipelineBuilder> so the caller owns the
 * facade exclusively (FF-1, INV-9).  All three references must outlive
 * the returned builder and any @ref IPipeline objects produced from it.
 */
[[nodiscard]] std::unique_ptr<IPipelineBuilder>
    createPipelineBuilder(vigine::messaging::IMessageBus       &bus,
                          vigine::threading::IThreadManager    &threadManager,
                          vigine::channelfactory::IChannelFactory &channelFactory);

} // namespace vigine::pipelinebuilder
