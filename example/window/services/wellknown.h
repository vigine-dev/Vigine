#pragma once

#include <vigine/api/service/serviceid.h>

namespace example::services::wellknown
{
/**
 * @file wellknown.h
 * @brief Application-scope well-known service ids for the
 *        example-window app.
 *
 * Application well-known ids live in the @c index >= 16 range so they
 * never overlap the engine-reserved range
 * (@c vigine::service::wellknown::* uses @c index in [1..15]).
 * Applications declare their own ids here and register matching
 * services through @c IContext::registerService(svc, knownId); tasks
 * resolve them through @c apiToken()->service(...) the same way they
 * resolve engine-scope services.
 */

/**
 * @brief Well-known id for the example's @c TextEditorService.
 *
 * Bound to the @c TextEditorService implementation that wraps the
 * @c TextEditState + @c TextEditorSystem pair the example's editor
 * tasks consume. The service is registered once in @c main and
 * resolved by every editor-touching task through this constant.
 */
inline constexpr vigine::service::ServiceId textEditor{16, 1};

} // namespace example::services::wellknown
