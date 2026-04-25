#pragma once

#include <memory>

#include "vigine/api/actorhost/abstractactorhost.h"
#include "vigine/api/actorhost/iactormailbox.h"

namespace vigine::actorhost
{

/**
 * @brief Concrete final actor-host facade.
 *
 * @ref ActorHost is Level-5 of the five-layer wrapper recipe.  It
 * provides the full @ref IActorHost implementation on top of
 * @ref AbstractActorHost:
 *
 *   - A generational id counter assigning fresh @ref ActorId values.
 *   - A per-actor registry mapping @ref ActorId to a live mailbox state.
 *   - One named thread per actor, registered with @ref IThreadManager, that
 *     drains the actor's @ref IMessageChannel and calls @ref IActor::receive
 *     sequentially.  This is the serialisation guarantee of the actor model.
 *   - Exception isolation: an exception escaping @ref IActor::receive is
 *     caught, logged, and the loop continues (v1 restart-not-supervised).
 *
 * Callers obtain instances exclusively through @ref createActorHost.
 *
 * Thread-safety: all public methods are safe to call concurrently.  The
 * actor registry is guarded by a @c std::mutex.  Each actor's mailbox
 * channel provides its own thread-safety.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - INV-9:  factory returns @c std::unique_ptr<IActorHost>.
 *   - INV-11: no graph types leak into this header.
 */
class ActorHost final : public AbstractActorHost
{
  public:
    /**
     * @brief Constructs the actor-host facade over @p bus and @p threadManager.
     *
     * Both @p bus and @p threadManager must outlive this facade instance.
     */
    ActorHost(vigine::messaging::IMessageBus    &bus,
              vigine::core::threading::IThreadManager &threadManager);

    ~ActorHost() override;

    // IActorHost
    [[nodiscard]] std::unique_ptr<IActorMailbox>
        spawn(std::unique_ptr<IActor> actor) override;

    [[nodiscard]] vigine::Result
        tell(ActorId id,
             std::unique_ptr<vigine::messaging::IMessage> message) override;

    void stop(ActorId id) override;

    void shutdown() override;

    ActorHost(const ActorHost &)            = delete;
    ActorHost &operator=(const ActorHost &) = delete;
    ActorHost(ActorHost &&)                  = delete;
    ActorHost &operator=(ActorHost &&)       = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::actorhost
