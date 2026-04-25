#pragma once

#include <cstdint>

namespace vigine::messaging
{
/**
 * @brief Closed enumeration of the kinds of message targets the engine
 *        routes messages to.
 *
 * Every concrete @ref AbstractMessageTarget reports one of these seven
 * tags so that the bus can disambiguate delivery policy without paying
 * for a @c dynamic_cast on the hot path. The enumeration is intentionally
 * closed: adding a new value is a breaking API change and requires
 * architect approval.
 *
 * Values:
 *   - @c State       -- a state in the state machine.
 *   - @c TaskFlow    -- a task-flow wrapper node.
 *   - @c Task        -- a concrete task scheduled inside a task flow.
 *   - @c TopicNode   -- a subscription target on a topic-shaped channel.
 *   - @c ChannelNode -- a generic channel-node subscriber.
 *   - @c ActorNode   -- an actor-shaped target with its own mailbox.
 *   - @c User        -- any target supplied by application code.
 */
enum class TargetKind : std::uint8_t
{
    State       = 1,
    TaskFlow    = 2,
    Task        = 3,
    TopicNode   = 4,
    ChannelNode = 5,
    ActorNode   = 6,
    User        = 7,
};

} // namespace vigine::messaging
