#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "vigine/messaging/busid.h"

namespace vigine::messaging
{
/**
 * @brief Closed enumeration of bus scheduling priorities.
 *
 * The priority is advisory: the thread manager uses it to bias worker
 * selection when more than one bus competes for a shared pool thread.
 * @c High is typically assigned to the system bus so that
 * @ref MessageKind::Control traffic wins against application workflows
 * when the pool is saturated; @c Normal and @c Low are the usual tiers
 * for application buses.
 */
enum class BusPriority : std::uint8_t
{
    High   = 1,
    Normal = 2,
    Low    = 3,
};

/**
 * @brief Closed enumeration of bus threading policies.
 *
 * Selects the execution model the bus uses to drain its dispatch queue.
 *   - @c Dedicated  -- the bus runs on its own worker, isolated from the
 *                      pool. Used by the system bus to keep lifecycle
 *                      traffic responsive under application load.
 *   - @c Shared     -- the bus schedules dispatch work on the thread
 *                      manager's shared pool. Default for user buses.
 *   - @c InlineOnly -- the bus has no worker; @ref IMessageBus::post
 *                      dispatches synchronously on the caller's thread.
 *                      Used by tests and deterministic embedded setups.
 */
enum class ThreadingPolicy : std::uint8_t
{
    Dedicated  = 1,
    Shared     = 2,
    InlineOnly = 3,
};

/**
 * @brief Closed enumeration of backpressure strategies applied when the
 *        dispatch queue reaches its @ref QueueCapacity::maxMessages cap.
 *
 *   - @c Block      -- the posting thread waits until the queue drains
 *                      below the high-water mark.
 *   - @c DropOldest -- the oldest still-queued message is discarded to
 *                      make room for the new one.
 *   - @c Error      -- @ref IMessageBus::post returns an error result
 *                      immediately; the caller decides how to retry.
 */
enum class BackpressurePolicy : std::uint8_t
{
    Block      = 1,
    DropOldest = 2,
    Error      = 3,
};

/**
 * @brief POD describing the size of the bus's dispatch queue.
 *
 * When @c bounded is @c true, the queue holds at most @c maxMessages
 * pending messages; additional @ref IMessageBus::post calls apply the
 * configured @ref BackpressurePolicy. When @c bounded is @c false the
 * queue grows dynamically and backpressure is effectively disabled.
 */
// ENCAP EXEMPT: pure value aggregate
struct QueueCapacity
{
    std::size_t maxMessages{1024};
    bool        bounded{true};
};

/**
 * @brief POD describing the shape of a single message bus.
 *
 * Passed to @ref createMessageBus at construction time; stored inside the
 * bus and reported back through @ref IMessageBus::config. All fields are
 * plain value types so the config survives a round-trip through any
 * facade that needs to inspect it without coupling to the bus's internal
 * state.
 *
 * The default values describe a user bus: @c Normal priority, shared
 * threading, 1024-deep bounded queue with blocking backpressure, and the
 * sentinel @ref BusId which the factory replaces with the next assigned
 * id. The @c name field is a non-owning view; callers must keep the
 * backing storage alive for the lifetime of the bus (typically a
 * string literal).
 */
// ENCAP EXEMPT: pure value aggregate
struct BusConfig
{
    BusId              id{};
    std::string_view   name{"user-bus"};
    BusPriority        priority{BusPriority::Normal};
    ThreadingPolicy    threading{ThreadingPolicy::Shared};
    QueueCapacity      capacity{};
    BackpressurePolicy backpressure{BackpressurePolicy::Block};
};

} // namespace vigine::messaging
