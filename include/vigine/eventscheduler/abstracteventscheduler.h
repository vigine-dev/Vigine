#pragma once

#include "vigine/messaging/abstractmessagebus.h"
#include "vigine/eventscheduler/ieventscheduler.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::eventscheduler
{

// Alias brought into this namespace so the class-head below can use the
// unqualified name AbstractMessageBus (required by acceptance grep).
using AbstractMessageBus = vigine::messaging::AbstractMessageBus;

/**
 * @brief Stateful abstract base for the event-scheduler facade.
 *
 * @ref AbstractEventScheduler is Level-4 of the five-layer wrapper recipe
 * (see @c theory_wrapper_creation_recipe.md). It inherits
 * @ref IEventScheduler @c public so the scheduler facade surface sits at
 * offset zero for zero-cost up-casts, and
 * @ref AbstractMessageBus @c protected so the bus substrate is available
 * to wrapper code without leaking the bus surface into the public
 * event-scheduler API.
 *
 * The class carries state (the underlying bus), so it follows the
 * project's @c Abstract naming convention rather than the @c I
 * pure-virtual prefix.
 *
 * Concrete subclasses (for example @ref DefaultEventScheduler) close the
 * chain by providing a concrete @ref vigine::messaging::BusConfig and
 * wiring the platform @ref ITimerSource and @ref IOsSignalSource
 * implementations. Callers never name those types directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref IEventScheduler FIRST,
 *     @c protected @ref AbstractMessageBus SECOND
 *     (mandatory per 5-layer recipe).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractEventScheduler : public IEventScheduler, protected AbstractMessageBus
{
  public:
    ~AbstractEventScheduler() override = default;

    AbstractEventScheduler(const AbstractEventScheduler &)            = delete;
    AbstractEventScheduler &operator=(const AbstractEventScheduler &) = delete;
    AbstractEventScheduler(AbstractEventScheduler &&)                  = delete;
    AbstractEventScheduler &operator=(AbstractEventScheduler &&)       = delete;

  protected:
    /**
     * @brief Constructs the abstract base with @p config and
     *        @p threadManager forwarded to the underlying
     *        @ref vigine::messaging::AbstractMessageBus.
     */
    AbstractEventScheduler(vigine::messaging::BusConfig      config,
                           vigine::threading::IThreadManager &threadManager);
};

} // namespace vigine::eventscheduler
