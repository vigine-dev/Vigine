#pragma once

#include <memory>

#include "vigine/ecs/iecs.h"

namespace vigine::ecs
{
/**
 * @brief Constructs the default concrete ECS and hands back an owning
 *        @c std::unique_ptr<IECS>.
 *
 * The factory is the single public entry point callers use to
 * instantiate an ECS in this leaf. The returned object is the minimal
 * concrete closer over @ref AbstractECS; it carries no domain-
 * specific behaviour of its own and exists so the wrapper primitive
 * can be exercised, linked, and tested in isolation.
 *
 * Ownership: the caller owns the returned pointer. Callers that need
 * shared ownership wrap the result in a @c std::shared_ptr at the
 * call site; shared ownership is not the factory's concern. This
 * mirrors the shape used by the service factory
 * (@ref vigine::service::createService), the thread manager factory
 * (@ref vigine::threading::createThreadManager), the payload registry
 * factory, and the message bus factory
 * (@ref vigine::messaging::createMessageBus).
 *
 * Lifetime: the returned ECS is self-contained. The internal entity
 * world is constructed eagerly during the factory call; every entity
 * created afterwards lives for the lifetime of the returned handle
 * (or until explicit removal via @ref IECS::removeEntity).
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would leak the allocation and leave the caller
 * with nothing — the motivation for the @ref FF-1 factory rule.
 */
[[nodiscard]] std::unique_ptr<IECS> createECS();

} // namespace vigine::ecs
