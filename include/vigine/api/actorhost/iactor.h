#pragma once

#include "vigine/messaging/imessage.h"
#include "vigine/result.h"

namespace vigine::actorhost
{

/**
 * @brief Pure-virtual actor-side lifecycle hooks.
 *
 * An @ref IActor is the user-supplied unit of isolated state.  The actor host
 * calls these hooks on the actor's dedicated mailbox thread, so no two calls
 * ever overlap for the same actor instance.
 *
 * Lifecycle:
 *   - @ref onStart  -- called once, before the first @ref receive.
 *   - @ref receive  -- called for each message taken from the mailbox.
 *   - @ref onStop   -- called once after @ref IActorMailbox::stop has drained
 *                      the in-flight @ref receive; never called concurrently
 *                      with @ref receive.
 *
 * Exception safety:
 *   An exception escaping @ref receive is caught by the host, logged, and the
 *   actor continues receiving subsequent messages (v1 restart-not-supervised
 *   semantics; full supervision deferred to v2).
 *
 * Invariants:
 *   - INV-1:  no template parameters.
 *   - INV-10: @c I prefix for a pure-virtual interface with no state.
 *   - INV-11: no graph types appear in this header.
 */
class IActor
{
  public:
    virtual ~IActor() = default;

    /**
     * @brief Called once before the first @ref receive.
     *
     * May perform initialisation that requires the actor's execution context
     * to be live.  A failure result is logged; the actor proceeds to the
     * receive loop regardless.
     */
    virtual vigine::Result onStart() { return vigine::Result{}; }

    /**
     * @brief Called for each message dequeued from the actor's mailbox.
     *
     * Implementations access the payload through the standard @ref IMessage
     * surface (kind, payloadTypeId, payload pointer).  An exception escaping
     * this method is caught by the host, logged, and the actor loop continues.
     */
    virtual vigine::Result receive(const vigine::messaging::IMessage &message) = 0;

    /**
     * @brief Called once after the final in-flight @ref receive completes.
     *
     * Implementations release actor-owned resources here.  The host waits for
     * @ref onStop to return before completing the stop sequence.
     */
    virtual void onStop() {}

    IActor(const IActor &)            = delete;
    IActor &operator=(const IActor &) = delete;
    IActor(IActor &&)                 = delete;
    IActor &operator=(IActor &&)      = delete;

  protected:
    IActor() = default;
};

} // namespace vigine::actorhost
