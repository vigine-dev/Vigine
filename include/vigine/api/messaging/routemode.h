#pragma once

#include <cstdint>

namespace vigine::messaging
{
/**
 * @brief Closed enumeration of the five routing strategies the bus
 *        supports.
 *
 * Every @ref IMessage reports a @ref RouteMode alongside its
 * @ref MessageKind. The bus's dispatch driver selects the matching
 * algorithm and walks the subscription registry accordingly. The set is
 * deliberately closed because each value has a documented algorithm and
 * a documented facade (see plan_09 routing matrix).
 *
 * Semantics:
 *   - @c FirstMatch -- depth-first walk with early exit on the first
 *                      subscriber that returns @ref DispatchResult::Handled.
 *   - @c FanOut     -- one-level breadth-first walk; every subscriber in
 *                      the immediate match set receives the message.
 *   - @c Chain      -- linear traversal until a subscriber returns
 *                      @ref DispatchResult::Handled or
 *                      @ref DispatchResult::Stop.
 *   - @c Bubble     -- parent-chain traversal. The dispatcher walks from
 *                      the message's target upward through its
 *                      @ref AbstractMessageTarget::parent() chain; when a
 *                      target reports no parent, @c Bubble degrades to
 *                      @c FirstMatch on the original target.
 *   - @c Broadcast  -- every subscriber across the whole registry
 *                      receives the message, regardless of filter target.
 */
enum class RouteMode : std::uint8_t
{
    FirstMatch = 1,
    FanOut     = 2,
    Chain      = 3,
    Bubble     = 4,
    Broadcast  = 5,
};

/**
 * @brief Closed enumeration of the three outcomes a subscriber reports
 *        back to the bus after processing a message.
 *
 * Returned from @ref ISubscriber::onMessage. The dispatch driver reads
 * this value to decide whether to keep walking (@c Pass), to stop after
 * this subscriber (@c Handled, early-exit for @c FirstMatch and
 * @c Chain), or to abort the current dispatch altogether (@c Stop).
 */
enum class DispatchResult : std::uint8_t
{
    Handled = 1,
    Pass    = 2,
    Stop    = 3,
};

} // namespace vigine::messaging
