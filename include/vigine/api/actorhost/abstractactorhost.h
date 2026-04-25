#pragma once

#include "vigine/api/actorhost/iactorhost.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/core/threading/ithreadmanager.h"

namespace vigine::actorhost
{

/**
 * @brief Stateful abstract base for the actor-host facade.
 *
 * @ref AbstractActorHost is Level-4 of the five-layer wrapper recipe.  It
 * inherits @ref IActorHost @c public so the actor surface sits at offset zero
 * for zero-cost up-casts, and holds references to the underlying
 * @ref vigine::messaging::IMessageBus and @ref vigine::core::threading::IThreadManager
 * @c protected so subclass wiring can reach them without exposing the raw
 * surfaces in the public actor-host API.
 *
 * The class carries state (two references), so it follows the project's
 * @c Abstract naming convention rather than the @c I pure-virtual prefix.
 *
 * Concrete subclasses (e.g. @ref ActorHost) close the chain by
 * providing the actor registry, the mailbox loop, and the full
 * @ref IActorHost implementation.  Callers never name those types directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref IActorHost FIRST (mandatory per
 *     5-layer recipe so the facade surface sits at offset zero).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractActorHost : public IActorHost
{
  public:
    ~AbstractActorHost() override = default;

    AbstractActorHost(const AbstractActorHost &)            = delete;
    AbstractActorHost &operator=(const AbstractActorHost &) = delete;
    AbstractActorHost(AbstractActorHost &&)                  = delete;
    AbstractActorHost &operator=(AbstractActorHost &&)       = delete;

  protected:
    /**
     * @brief Constructs the abstract base holding references to @p bus and
     *        @p threadManager.
     *
     * The caller (factory or test harness) guarantees both references outlive
     * this facade instance.
     */
    AbstractActorHost(vigine::messaging::IMessageBus    &bus,
                      vigine::core::threading::IThreadManager &threadManager);

    /**
     * @brief Returns the underlying bus reference for subclass wiring.
     */
    [[nodiscard]] vigine::messaging::IMessageBus &bus() noexcept;

    /**
     * @brief Returns the thread manager reference for subclass wiring.
     */
    [[nodiscard]] vigine::core::threading::IThreadManager &threadManager() noexcept;

  private:
    vigine::messaging::IMessageBus    &_bus;
    vigine::core::threading::IThreadManager &_threadManager;
};

} // namespace vigine::actorhost
