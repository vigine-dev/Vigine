#pragma once

#include "vigine/messaging/abstractmessagebus.h"
#include "vigine/messaging/busconfig.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::messaging
{
/**
 * @brief Concrete message bus that closes the five-layer wrapper recipe.
 *
 * @ref SystemMessageBus is the default concrete bus; its @c final
 * keyword prevents further subclassing so that @ref createMessageBus
 * has one well-defined return type. The class inherits the full
 * @ref AbstractMessageBus implementation and adds no storage or
 * behaviour of its own; its role is only to seal the chain and, when
 * built with the system-bus preset, pin the config to the reserved
 * @c BusId @c == @c 1 tier that the engine uses for lifecycle traffic.
 *
 * The constructor overload without an explicit @ref BusId picks system-
 * bus defaults (high priority, dedicated thread, bounded-1024 queue,
 * blocking backpressure). The overload that takes an explicit
 * @ref BusConfig honours the caller-supplied shape verbatim.
 */
class SystemMessageBus final : public AbstractMessageBus
{
  public:
    /**
     * @brief Builds the system bus with the preset configuration.
     *
     * The preset pins the bus id to the reserved system value
     * (@c 1), picks @ref BusPriority::High,
     * @ref ThreadingPolicy::Dedicated, a 1024-deep bounded queue, and
     * @ref BackpressurePolicy::Block. Use this overload when wiring the
     * engine's single system bus; use @ref createMessageBus for user
     * buses with arbitrary configs.
     */
    explicit SystemMessageBus(vigine::core::threading::IThreadManager &threadManager);

    /**
     * @brief Builds the bus with @p config verbatim.
     *
     * Used by @ref createMessageBus to construct user buses with
     * caller-supplied shapes. The factory assigns a fresh
     * @ref BusId before handing the config in, so callers supplying the
     * sentinel zero end up with an auto-assigned id.
     */
    SystemMessageBus(BusConfig                          config,
                     vigine::core::threading::IThreadManager &threadManager);

    ~SystemMessageBus() override = default;

    SystemMessageBus(const SystemMessageBus &)            = delete;
    SystemMessageBus &operator=(const SystemMessageBus &) = delete;
    SystemMessageBus(SystemMessageBus &&)                 = delete;
    SystemMessageBus &operator=(SystemMessageBus &&)      = delete;
};

} // namespace vigine::messaging
