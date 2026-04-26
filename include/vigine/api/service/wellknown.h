#pragma once

#include "vigine/api/service/serviceid.h"

namespace vigine::service::wellknown
{
/**
 * @file wellknown.h
 * @brief Engine-reserved well-known @ref ServiceId constants.
 *
 * The well-known constants below identify services the engine itself
 * registers under stable, caller-known ids during context construction.
 * Tasks that depend on those services resolve them by these constants
 * through the engine-token's @c service() accessor without anyone
 * threading a runtime-allocated id through the wiring path.
 *
 * Id-space convention:
 *   - @c index in [1..15] is reserved for the engine. Adding a new
 *     engine-scope well-known id picks the next free slot in this
 *     range and bumps the @c generation field with every breaking
 *     change to its contract.
 *   - @c index >= 16 is free for application-side well-known ids.
 *     Applications declare their own constants in their own
 *     namespace (e.g. @c myapp::services::wellknown) and register
 *     services through @c IContext::registerService(svc, knownId).
 *   - The @c generation field starts at @c 1 for every freshly-
 *     declared id; bump it (and the matching @c registerService
 *     call site) when a new service implementation replaces an
 *     incompatible predecessor under the same slot.
 *
 * Lookups always go through the engine token:
 * @code
 *   auto svc = apiToken()->service(vigine::service::wellknown::platformService);
 *   if (!svc.ok()) { ... }
 *   auto* platformService =
 *       dynamic_cast<vigine::ecs::platform::PlatformService*>(&svc.value());
 * @endcode
 *
 * The engine-built defaults are auto-registered by @ref AbstractContext
 * during construction; applications that want a different concrete
 * implementation call @c IContext::registerService(myImpl, knownId)
 * which replaces the slot's occupant and destroys the prior default
 * via the registry's RAII chain.
 */

/**
 * @brief Well-known id for the engine-wide @c PlatformService.
 *
 * The default is built by @ref AbstractContext with a default
 * @c WindowSystem("DefaultWindow") owned internally. Applications
 * that need a different concrete platform implementation register
 * their own under this id; the default is destroyed in the process.
 */
inline constexpr ServiceId platformService{1, 1};

/**
 * @brief Well-known id for the engine-wide @c GraphicsService.
 *
 * The default is built by @ref AbstractContext with a default
 * @c RenderSystem("DefaultRender") owned internally. Override pattern
 * mirrors @ref platformService.
 */
inline constexpr ServiceId graphicsService{2, 1};

} // namespace vigine::service::wellknown
