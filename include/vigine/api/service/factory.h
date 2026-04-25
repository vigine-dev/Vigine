#pragma once

#include <memory>

#include "vigine/api/service/iservice.h"

namespace vigine::service
{
/**
 * @brief Constructs the default concrete service and hands back an
 *        owning @c std::unique_ptr<IService>.
 *
 * The factory is the single public entry point callers use to
 * instantiate a service in this leaf. The returned object is the
 * minimal concrete closer over @ref AbstractService; it carries no
 * domain-specific behaviour of its own and exists so the wrapper
 * primitive can be exercised, linked, and tested in isolation.
 * Domain-specific services (graphics, platform, network, database,
 * timer, ...) land in dedicated follow-up leaves and each provide
 * their own factory entry point that shares this signature.
 *
 * Ownership: the caller owns the returned pointer. Callers that need
 * shared ownership wrap the result in a @c std::shared_ptr at the
 * call site; shared ownership is not the factory's concern. This
 * mirrors the shape used by the thread manager factory
 * (@ref vigine::core::threading::createThreadManager), the payload registry
 * factory, and the message bus factory
 * (@ref vigine::messaging::createMessageBus).
 *
 * Lifetime: the returned service is self-contained. The engine takes
 * ownership during registration; before registration the service is
 * idle and its @ref IService::id reports the invalid sentinel.
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would leak the allocation and leave the caller with
 * nothing — the motivation for the @ref FF-1 factory rule.
 */
[[nodiscard]] std::unique_ptr<IService> createService();

} // namespace vigine::service
