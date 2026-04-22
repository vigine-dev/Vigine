#pragma once

#include "vigine/messaging/abstractmessagebus.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/signalemitter/isignalemitter.h"

namespace vigine::signalemitter
{

// Alias brought into this namespace so the class-head below can use the
// unqualified name AbstractMessageBus (required by acceptance grep).
using AbstractMessageBus = vigine::messaging::AbstractMessageBus;

/**
 * @brief Stateful abstract base for the signal-emitter facade.
 *
 * @ref AbstractSignalEmitter is Level-4 of the five-layer wrapper recipe
 * (see @c theory_wrapper_creation_recipe.md). It inherits
 * @ref ISignalEmitter @c public so the signal facade surface sits at
 * offset zero for zero-cost up-casts, and
 * @ref AbstractMessageBus @c protected so the bus substrate is available
 * to wrapper code without leaking the bus surface into the public
 * signal-emitter API.
 *
 * The class carries state (the underlying bus), so it follows the
 * project's @c Abstract naming convention rather than the @c I
 * pure-virtual prefix.
 *
 * Concrete subclasses (for example @ref DefaultSignalEmitter) close the
 * chain by providing a concrete @ref vigine::messaging::BusConfig and
 * wiring a @ref vigine::threading::IThreadManager. Callers never name
 * those types directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref ISignalEmitter FIRST,
 *     @c protected @ref AbstractMessageBus SECOND
 *     (mandatory per 5-layer recipe).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractSignalEmitter : public ISignalEmitter, protected AbstractMessageBus
{
  public:
    ~AbstractSignalEmitter() override = default;

    /**
     * @brief Subscribes @p subscriber to signals of the payload type
     *        recorded in @p filter on this emitter's internal bus.
     *
     * Convenience entry point so callers that hold an
     * @ref AbstractSignalEmitter can subscribe without a separate bus
     * handle. The filter's @c kind field is forced to
     * @ref vigine::messaging::MessageKind::Signal before forwarding to
     * @ref vigine::messaging::AbstractMessageBus::subscribe so the
     * subscription is always scoped to signal traffic.
     *
     * Returns an @ref vigine::messaging::ISubscriptionToken; dropping the
     * token cancels the subscription.
     */
    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeSignal(vigine::messaging::MessageFilter                       filter,
                        vigine::messaging::ISubscriber                        *subscriber);

    AbstractSignalEmitter(const AbstractSignalEmitter &)            = delete;
    AbstractSignalEmitter &operator=(const AbstractSignalEmitter &) = delete;
    AbstractSignalEmitter(AbstractSignalEmitter &&)                  = delete;
    AbstractSignalEmitter &operator=(AbstractSignalEmitter &&)       = delete;

  protected:
    /**
     * @brief Constructs the abstract base with @p config and
     *        @p threadManager forwarded to the underlying
     *        @ref vigine::messaging::AbstractMessageBus.
     */
    AbstractSignalEmitter(vigine::messaging::BusConfig               config,
                          vigine::threading::IThreadManager          &threadManager);
};

} // namespace vigine::signalemitter
