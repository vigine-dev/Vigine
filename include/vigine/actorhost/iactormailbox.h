#pragma once

#include "vigine/actorhost/actorid.h"
#include "vigine/result.h"

namespace vigine::actorhost
{

/**
 * @brief Per-actor FIFO mailbox handle returned by @ref IActorHost::spawn.
 *
 * The mailbox owns the actor's execution context (a dedicated thread or
 * serialised worker) and its message queue.  Dropping the handle or calling
 * @ref stop terminates the actor in an orderly fashion.
 *
 * Invariants:
 *   - INV-1:  no template parameters.
 *   - INV-10: @c I prefix for a pure-virtual interface.
 *   - INV-11: no graph types appear in this header.
 */
class IActorMailbox
{
  public:
    virtual ~IActorMailbox() = default;

    /**
     * @brief Returns the stable @ref ActorId assigned to this mailbox by the
     *        host at spawn time.
     */
    [[nodiscard]] virtual ActorId actorId() const noexcept = 0;

    /**
     * @brief Closes the mailbox and blocks until any in-flight @ref receive
     *        and @ref IActor::onStop complete.
     *
     * After @ref stop returns, subsequent @ref IActorHost::tell calls for
     * this id return @c Result::Code::Error.  Idempotent.
     */
    virtual void stop() = 0;

    IActorMailbox(const IActorMailbox &)            = delete;
    IActorMailbox &operator=(const IActorMailbox &) = delete;
    IActorMailbox(IActorMailbox &&)                 = delete;
    IActorMailbox &operator=(IActorMailbox &&)      = delete;

  protected:
    IActorMailbox() = default;
};

} // namespace vigine::actorhost
