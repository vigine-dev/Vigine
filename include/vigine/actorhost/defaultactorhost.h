#pragma once

#include <memory>

#include "vigine/actorhost/abstractactorhost.h"
#include "vigine/actorhost/iactormailbox.h"

namespace vigine::actorhost
{

/**
 * @brief Concrete final actor-host facade.
 *
 * @ref DefaultActorHost is Level-5 of the five-layer wrapper recipe.  It
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
class DefaultActorHost final : public AbstractActorHost
{
  public:
    /**
     * @brief Constructs the actor-host facade over @p bus and @p threadManager.
     *
     * Both @p bus and @p threadManager must outlive this facade instance.
     */
    DefaultActorHost(vigine::messaging::IMessageBus    &bus,
                     vigine::threading::IThreadManager &threadManager);

    ~DefaultActorHost() override;

    // IActorHost
    [[nodiscard]] std::unique_ptr<IActorMailbox>
        spawn(std::unique_ptr<IActor> actor) override;

    [[nodiscard]] vigine::Result
        tell(ActorId id,
             std::unique_ptr<vigine::messaging::IMessage> message) override;

    void stop(ActorId id) override;

    void shutdown() override;

    DefaultActorHost(const DefaultActorHost &)            = delete;
    DefaultActorHost &operator=(const DefaultActorHost &) = delete;
    DefaultActorHost(DefaultActorHost &&)                  = delete;
    DefaultActorHost &operator=(DefaultActorHost &&)       = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Factory function — the sole entry point for creating an actor-host
 *        facade.
 *
 * Returns a @c std::unique_ptr<IActorHost> so the caller owns the facade
 * exclusively (FF-1, INV-9).  Both @p bus and @p threadManager must outlive
 * the returned facade.
 */
[[nodiscard]] std::unique_ptr<IActorHost>
    createActorHost(vigine::messaging::IMessageBus    &bus,
                    vigine::threading::IThreadManager &threadManager);

} // namespace vigine::actorhost
