#pragma once

#include <memory>

#include "vigine/actorhost/actorid.h"
#include "vigine/actorhost/iactor.h"
#include "vigine/actorhost/iactormailbox.h"
#include "vigine/messaging/imessage.h"
#include "vigine/result.h"

namespace vigine::actorhost
{

/**
 * @brief Pure-virtual actor-host facade over @ref vigine::messaging::IMessageBus.
 *
 * @ref IActorHost is the Level-2 facade for the actor model (plan_21,
 * R.3.3.2.3).  It manages a collection of isolated @ref IActor instances, each
 * with its own FIFO mailbox processed by a single dedicated thread.  The
 * invariant that @ref IActor::receive is never called concurrently for the same
 * actor is enforced by the host through @ref vigine::core::threading::IThreadManager.
 *
 * Ownership:
 *   - @ref spawn takes unique ownership of the supplied @ref IActor and returns
 *     a @c std::unique_ptr<IActorMailbox>.  The mailbox handle is the RAII
 *     owner of the actor's execution context.
 *   - Dropping the returned mailbox or calling @ref IActorMailbox::stop drains
 *     any queued messages and joins the actor thread.
 *   - @ref tell borrows the bus to post an @ref IMessage addressed to the actor
 *     identified by @p id.  Posting to an already-stopped id returns an error.
 *   - @ref shutdown drains all mailboxes, joins all actor threads, and rejects
 *     subsequent operations.  Idempotent.
 *
 * Thread-safety: all entry points are safe to call concurrently.
 *
 * Invariants:
 *   - INV-1:  no template parameters in the public surface.
 *   - INV-9:  factory @ref createActorHost returns @c std::unique_ptr<IActorHost>.
 *   - INV-10: @c I prefix for this pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IActorHost
{
  public:
    virtual ~IActorHost() = default;

    /**
     * @brief Spawns a new actor and returns its mailbox handle.
     *
     * The host assigns a fresh generational @ref ActorId, calls
     * @ref IActor::onStart on the actor's thread, and begins draining the
     * mailbox queue.  A null @p actor is a programming error; callers must
     * supply a valid instance.
     *
     * Returns @c nullptr when the host is shut down.
     */
    [[nodiscard]] virtual std::unique_ptr<IActorMailbox>
        spawn(std::unique_ptr<IActor> actor) = 0;

    /**
     * @brief Enqueues @p message for delivery to the actor identified by @p id.
     *
     * Returns a successful @ref Result when the message was accepted.
     * Returns an error @ref Result when:
     *   - @p id is invalid or belongs to a stopped actor.
     *   - The host is shut down.
     *   - The underlying message bus rejects the post.
     */
    [[nodiscard]] virtual vigine::Result
        tell(ActorId id, std::unique_ptr<vigine::messaging::IMessage> message) = 0;

    /**
     * @brief Stops the actor identified by @p id and blocks until it drains.
     *
     * Equivalent to calling @ref IActorMailbox::stop on the handle returned by
     * the matching @ref spawn.  A stale or invalid @p id is a no-op.
     */
    virtual void stop(ActorId id) = 0;

    /**
     * @brief Drains all mailboxes and shuts down the host.
     *
     * Idempotent: a second call is a no-op.  After shutdown, @ref spawn
     * returns @c nullptr and @ref tell returns an error.
     */
    virtual void shutdown() = 0;

    IActorHost(const IActorHost &)            = delete;
    IActorHost &operator=(const IActorHost &) = delete;
    IActorHost(IActorHost &&)                 = delete;
    IActorHost &operator=(IActorHost &&)      = delete;

  protected:
    IActorHost() = default;
};

} // namespace vigine::actorhost
