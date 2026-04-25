#pragma once

#include <memory>

#include "vigine/api/requestbus/irequestbus.h"

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::requestbus
{
/**
 * @brief Factory function -- the sole public entry point for creating
 *        a request-bus facade.
 *
 * Returns a `std::unique_ptr<IRequestBus>` so the caller owns the
 * facade exclusively. Both `@p bus` and `@p threadManager` must
 * outlive the returned facade; the facade keeps non-owning
 * references to each.
 *
 * This header is factory-only on purpose: it forward-declares the
 * two ref-argument types and includes only `irequestbus.h`, so
 * callers that want just the factory do not pull the concrete
 * `RequestBus` class body into their translation units.
 * Callers that need to name the concrete type can still include
 * `vigine/impl/requestbus/requestbus.h` directly.
 */
[[nodiscard]] std::unique_ptr<IRequestBus>
    createRequestBus(vigine::messaging::IMessageBus    &bus,
                     vigine::core::threading::IThreadManager &threadManager);

} // namespace vigine::requestbus
