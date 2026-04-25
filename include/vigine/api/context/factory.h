#pragma once

#include <memory>

#include "vigine/api/context/contextconfig.h"
#include "vigine/api/context/icontext.h"

namespace vigine::context
{
/**
 * @brief Constructs the default concrete @ref IContext and hands back
 *        an owning @c std::unique_ptr.
 *
 * The factory is the single public entry point callers use to build an
 * aggregator. It wires the strict construction chain encoded on
 * @ref AbstractContext:
 *
 *   1. Build the thread manager from @p config.threading.
 *   2. Build the system bus from @p config.systemBus, wiring the
 *      thread manager in.
 *   3. Default-construct the three Level-1 wrappers (ECS, state
 *      machine, task flow).
 *   4. Open the user-bus and service registries (empty).
 *
 * Services are not part of the factory call; the caller registers
 * them through @ref IContext::registerService after the context is
 * built. The engine calls @ref IContext::freeze once the main loop is
 * about to start; after the freeze point, registration returns
 * @ref Result::Code::TopologyFrozen.
 *
 * Ownership: the caller owns the returned pointer. The signature is
 * @c std::unique_ptr because the context is a singular owner inside
 * the engine construction chain (FF-1). Callers that need shared
 * ownership lift the returned pointer into a @c std::shared_ptr at
 * the call site; shared ownership is not the factory's concern.
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would tear down every wrapper the factory just
 * constructed -- wasted allocation and immediate join of the thread
 * manager pool.
 */
[[nodiscard]] std::unique_ptr<IContext>
    createContext(const ContextConfig &config = {});

} // namespace vigine::context
