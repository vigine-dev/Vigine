#pragma once

#include <memory>

#include "vigine/reactivestream/ireactivestream.h"

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::reactivestream
{
/**
 * @brief Factory function — the sole public entry point for creating
 *        a reactive-stream facade.
 *
 * Returns a `std::unique_ptr<IReactiveStream>` so the caller owns the
 * facade exclusively. Both `@p bus` and `@p threadManager` must
 * outlive the returned facade; the facade keeps non-owning references
 * to each.
 *
 * This header matches the factory convention used by every other
 * facade (signal / topic / channel / request / actor / pipeline /
 * event scheduler): the public factory is a thin, interface-only
 * header that deliberately does not pull the concrete type into
 * callers' translation units. Callers that want just the factory
 * include this file; callers that need to name the concrete type
 * (tests, engineering) can still reach it through
 * `defaultreactivestream.h`.
 */
[[nodiscard]] std::unique_ptr<IReactiveStream>
    createReactiveStream(vigine::messaging::IMessageBus    &bus,
                         vigine::core::threading::IThreadManager &threadManager);

} // namespace vigine::reactivestream
