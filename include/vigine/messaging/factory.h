#pragma once

#include <memory>

#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/imessagebus.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::messaging
{
/**
 * @brief Constructs a new concrete @ref IMessageBus and returns a unique
 *        owning pointer.
 *
 * The factory is the single entry point callers use to instantiate a
 * bus; every concrete implementation lives under @c src/messaging and is
 * selected at build time by the engine library. The factory validates
 * @p config (applies defaults for the sentinel @c BusId, clamps the
 * queue capacity when unbounded mode is requested with a zero cap),
 * builds the control block, and wires the bus to the supplied
 * @p threadManager so that dispatch workers run on the engine's shared
 * threading substrate.
 *
 * Ownership: the caller owns the returned pointer. Callers that need
 * shared ownership wrap the result in a @c std::shared_ptr at the call
 * site; shared ownership is not the factory's concern. This mirrors the
 * thread manager's factory (see
 * @ref vigine::core::threading::createThreadManager) and the payload
 * registry's factory (see @ref vigine::payload::createPayloadRegistry).
 *
 * Lifetime: the returned bus retains a reference to @p threadManager.
 * The caller must guarantee the manager outlives every bus the factory
 * constructs with it; the engine enforces this by destroying buses
 * strictly before the thread manager during engine teardown.
 */
[[nodiscard]] std::unique_ptr<IMessageBus>
    createMessageBus(const BusConfig                  &config,
                     vigine::core::threading::IThreadManager &threadManager);

} // namespace vigine::messaging
