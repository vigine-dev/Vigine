#pragma once

#include <memory>

#include "vigine/api/topicbus/itopicbus.h"

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::topicbus
{
/**
 * @brief Factory function -- the sole public entry point for creating
 *        a topic-bus facade.
 *
 * Returns a `std::unique_ptr<ITopicBus>` so the caller owns the
 * facade exclusively. The supplied `@p bus` must outlive the
 * returned facade; the facade keeps a non-owning reference to it.
 *
 * This header is factory-only on purpose: it forward-declares the
 * bus-ref argument type and includes only `itopicbus.h`, so callers
 * that want just the factory do not pull the concrete `TopicBus`
 * class body into their translation units.
 * Callers that need to name the concrete type can still include
 * `vigine/impl/topicbus/topicbus.h` directly.
 */
[[nodiscard]] std::unique_ptr<ITopicBus>
    createTopicBus(vigine::messaging::IMessageBus &bus);

} // namespace vigine::topicbus
