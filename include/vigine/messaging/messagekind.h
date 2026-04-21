#pragma once

#include <cstdint>

namespace vigine::messaging
{
/**
 * @brief Closed enumeration of the nine message kinds carried by the
 *        engine's unified bus.
 *
 * Every @ref IMessage reports one of these tags. The bus uses the tag to
 * reject out-of-contract traffic on @ref IMessageBus::post and to let
 * facades filter subscriptions without paying for a @c dynamic_cast.
 * The set is intentionally closed: adding a new value is a breaking API
 * change and requires coordinated review -- each value has a documented
 * facade (see plan_09 routing matrix) and adding a tenth value without a
 * new facade breaks the facade <-> kind invariant.
 *
 * Mapping to facades (documented for traceability):
 *   - @c Signal         -- one-shot notification, typically @c FirstMatch.
 *   - @c Event          -- timed event; @c FirstMatch or @c Broadcast.
 *   - @c TopicPublish   -- publish on a topic, @c FanOut fan-out.
 *   - @c TopicRequest   -- request/response, @c FirstMatch.
 *   - @c ChannelSend    -- channel-send, @c FirstMatch.
 *   - @c ReactiveSignal -- reactive stream element, @c FanOut.
 *   - @c ActorMail      -- actor mailbox delivery, @c FirstMatch.
 *   - @c PipelineStep   -- pipeline stage advance, @c Chain.
 *   - @c Control        -- engine lifecycle traffic, @c Broadcast.
 */
enum class MessageKind : std::uint8_t
{
    Signal         = 1,
    Event          = 2,
    TopicPublish   = 3,
    TopicRequest   = 4,
    ChannelSend    = 5,
    ReactiveSignal = 6,
    ActorMail      = 7,
    PipelineStep   = 8,
    Control        = 9,
};

} // namespace vigine::messaging
