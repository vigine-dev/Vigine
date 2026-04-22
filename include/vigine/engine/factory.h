#pragma once

#include <memory>

#include "vigine/engine/engineconfig.h"
#include "vigine/engine/iengine.h"

namespace vigine::engine
{
/**
 * @brief Constructs the default concrete @ref IEngine and hands back
 *        an owning @c std::unique_ptr.
 *
 * The factory is the single public entry point callers use to build an
 * engine. It wires the strict construction chain encoded on
 * @ref AbstractEngine:
 *
 *   1. Build the context aggregator from @p config.context (which in
 *      turn builds the thread manager, the system bus, and the
 *      Level-1 wrappers in the AD-5 C8 order).
 *   2. Capture the run-mode hint and default-initialise the
 *      lifecycle flags.
 *
 * Services are not part of the factory call; the caller registers
 * them on the returned engine's @ref IEngine::context between
 * @ref createEngine and @ref IEngine::run. @ref IEngine::run calls
 * @ref IContext::freeze on entry; after the freeze point,
 * registration returns @ref Result::Code::TopologyFrozen.
 *
 * Ownership: the caller owns the returned pointer. The signature is
 * @c std::unique_ptr because the engine is a singular owner in the
 * engine construction chain (FF-1). Callers that need shared
 * ownership lift the returned pointer into a @c std::shared_ptr at
 * the call site; shared ownership is not the factory's concern.
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would tear down every wrapper the factory just
 * constructed -- wasted allocation and immediate join of the thread
 * manager pool.
 */
[[nodiscard]] std::unique_ptr<IEngine>
    createEngine(const EngineConfig &config = {});

} // namespace vigine::engine
