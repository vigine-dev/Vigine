#pragma once

#include <memory>

#include "vigine/api/pipelinebuilder/ipipelinebuilder.h"

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::channelfactory
{
class IChannelFactory;
} // namespace vigine::channelfactory

namespace vigine::pipelinebuilder
{
/**
 * @brief Factory function -- the sole public entry point for creating
 *        a pipeline-builder facade.
 *
 * Returns a `std::unique_ptr<IPipelineBuilder>` so the caller owns the
 * facade exclusively. All three references must outlive the returned
 * builder and any @ref IPipeline objects produced from it.
 *
 * This header is factory-only on purpose: it forward-declares the
 * three ref-argument types and includes only `ipipelinebuilder.h`,
 * so callers that want just the factory do not pull the concrete
 * `PipelineBuilder` class body into their translation units.
 * Callers that need to name the concrete type can still include
 * `vigine/impl/pipelinebuilder/pipelinebuilder.h` directly.
 */
[[nodiscard]] std::unique_ptr<IPipelineBuilder>
    createPipelineBuilder(vigine::messaging::IMessageBus       &bus,
                          vigine::core::threading::IThreadManager &threadManager,
                          vigine::channelfactory::IChannelFactory &channelFactory);

} // namespace vigine::pipelinebuilder
