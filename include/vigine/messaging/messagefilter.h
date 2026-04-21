#pragma once

#include <optional>

#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"

namespace vigine::messaging
{
class AbstractMessageTarget;

/**
 * @brief POD describing which messages a subscriber wants to see.
 *
 * Passed to @ref IMessageBus::subscribe. Every field is optional, except
 * for @c kind which is mandatory: the closed @ref MessageKind enum is the
 * first axis the bus filters on and has no "wildcard" value.
 *
 * Semantics of the optional fields:
 *   - @c typeId -- when the wrapped @c value is non-zero, the bus only
 *     delivers messages whose payload id matches exactly. A zero value
 *     accepts any payload id (wildcard).
 *   - @c target -- when set, the bus only delivers messages addressed
 *     to the matching @ref AbstractMessageTarget (pointer identity).
 *     @c std::nullopt accepts messages addressed to any target and
 *     broadcast-style messages with a null target pointer.
 *   - @c expectedRoute -- when set, the bus only delivers messages
 *     whose @ref IMessage::routeMode matches. Most subscribers leave
 *     this empty and trust the facade's route choice.
 *
 * The filter does not own the @c target pointer; the caller must keep
 * the target alive for as long as the subscription is active. This is
 * the same lifetime guarantee that @ref AbstractMessageTarget provides
 * through its RAII token vector: a target registered with any bus
 * outlives every subscription that references it.
 */
struct MessageFilter
{
    MessageKind                          kind{MessageKind::Signal};
    vigine::payload::PayloadTypeId       typeId{};
    const AbstractMessageTarget         *target{nullptr};
    std::optional<RouteMode>             expectedRoute{std::nullopt};
};

} // namespace vigine::messaging
