#pragma once

#include <memory>

#include "vigine/payload/ipayloadregistry.h"

namespace vigine::payload
{
/**
 * @brief Constructs the default @ref IPayloadRegistry implementation.
 *
 * Returns a unique owning pointer to the concrete engine-provided
 * registry. The factory is deliberately non-templated; every
 * implementation is selected at build time by the engine library.
 *
 * The returned registry has its four engine-bundled ranges (Control,
 * System, SystemExt, Reserved — see @ref payloadrange.h) pre-registered
 * under the `vigine.core` owner, so first-use lookups for any engine
 * identifier succeed without any application wiring.
 *
 * A @c unique_ptr is used — not a @c shared_ptr — because the registry
 * is a singular owner inside the engine construction chain. Callers
 * that need shared ownership can lift the returned pointer into a
 * @c shared_ptr at the call site.
 */
[[nodiscard]] std::unique_ptr<IPayloadRegistry> createPayloadRegistry();

} // namespace vigine::payload
