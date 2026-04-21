#pragma once

#include <memory>

#include "vigine/messaging/ibuscontrolblock.h"

namespace vigine::messaging
{
/**
 * @brief Drains a bus's pending connections at engine-shutdown time.
 *
 * Exposes a small helper the Engine can call on shutdown to flip an
 * @ref IBusControlBlock into the dead state before any remaining
 * @ref ConnectionToken destructors run. Declared as a standalone
 * helper because the Engine assembly leaf (context_aggregator,
 * plan_23) is the site that wires the exact dtor order; until that
 * lands, facade and bus ownership is introduced in later leaves and
 * this helper is available for them to call.
 *
 * Thread-safety: @ref flipDead is safe to call from any thread.
 * Idempotent: a second call on the same block is a no-op.
 */
class MessagingCleanup
{
  public:
    /**
     * @brief Flips @p block into the dead state.
     *
     * Null @p block is tolerated so callers can pass a bus-owned
     * @c std::shared_ptr without a pre-check. After this call every
     * live @ref ConnectionToken observing @p block reports
     * @ref IConnectionToken::active as @c false.
     */
    static void flipDead(const std::shared_ptr<IBusControlBlock> &block) noexcept;

    MessagingCleanup()                                     = delete;
    MessagingCleanup(const MessagingCleanup &)             = delete;
    MessagingCleanup &operator=(const MessagingCleanup &)  = delete;
    MessagingCleanup(MessagingCleanup &&)                  = delete;
    MessagingCleanup &operator=(MessagingCleanup &&)       = delete;
};

} // namespace vigine::messaging
