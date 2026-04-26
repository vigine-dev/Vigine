#pragma once

#include "vigine/api/messaging/busconfig.h"
#include "vigine/core/threading/threadmanagerconfig.h"

namespace vigine::context
{
/**
 * @brief POD describing the shape of a freshly-built @ref IContext.
 *
 * Passed to @ref createContext at construction time. Carries the two
 * inputs the aggregator needs before it can wire the Level-1 wrappers
 * together:
 *
 *   1. @ref threading configures the first member built by the
 *      aggregator (the thread manager).
 *   2. @ref systemBus configures the system bus that the aggregator
 *      builds next, wiring the thread manager in.
 *
 * The remaining wrappers (ECS, state machine, task flow) are built via
 * their default factories and do not require config fields here. User
 * services are registered through @ref IContext::registerService after
 * the context is constructed, so they are not part of this struct.
 *
 * Defaults describe an engine-default context: the default-constructed
 * thread-manager config (hardware-concurrency pool) and a system bus
 * config with the conventional "system" name and high priority. The
 * caller overrides individual fields before handing the struct to the
 * factory.
 */
// ENCAP EXEMPT: pure value aggregate
struct ContextConfig
{
    /**
     * @brief Threading substrate configuration consumed first.
     */
    core::threading::ThreadManagerConfig threading{};

    /**
     * @brief System bus configuration consumed second.
     *
     * The conventional name for the system bus is @c "system" and the
     * conventional priority is @ref messaging::BusPriority::High so
     * lifecycle traffic wins against application workload. Defaults
     * here match that expectation. The `BusId` defaults to the
     * engine-reserved system-bus value so a `ContextConfig{}` produces
     * a context that constructs its system bus without any further
     * caller input; previously this field defaulted to the invalid
     * sentinel `BusId{}`, which quietly produced a context that
     * failed to wire the system bus.
     */
    messaging::BusConfig systemBus{
        messaging::BusId{messaging::BusId::kSystemBusValue},
        "system",
        messaging::BusPriority::High,
        messaging::ThreadingPolicy::Dedicated,
        messaging::QueueCapacity{},
        messaging::BackpressurePolicy::Block
    };
};

} // namespace vigine::context
