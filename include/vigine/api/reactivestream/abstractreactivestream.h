#pragma once

#include "vigine/api/reactivestream/ireactivestream.h"
#include "vigine/messaging/imessagebus.h"

namespace vigine::reactivestream
{

/**
 * @brief Stateful abstract base for the reactive-stream facade.
 *
 * @ref AbstractReactiveStream is Level-4 of the five-layer wrapper recipe.
 * It inherits @ref IReactiveStream @c public (FIRST — facade surface at
 * offset zero for zero-cost up-casts) and holds a
 * @ref vigine::messaging::IMessageBus @c & @c private so the bus substrate
 * is accessible to subclasses through @ref bus() without leaking the raw
 * bus surface into the public reactive-stream API.
 *
 * The class carries state (the bus reference), so it follows the
 * project's @c Abstract naming convention rather than the @c I
 * pure-virtual prefix.
 *
 * Concrete subclasses (for example @ref ReactiveStream) close the
 * chain by providing the subscription registry, demand tracking, and the
 * full @ref IReactiveStream implementation. Callers never name those types
 * directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref IReactiveStream FIRST (mandatory
 *     per 5-layer recipe so the facade surface sits at offset zero).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractReactiveStream : public IReactiveStream
{
  public:
    ~AbstractReactiveStream() override = default;

    AbstractReactiveStream(const AbstractReactiveStream &)            = delete;
    AbstractReactiveStream &operator=(const AbstractReactiveStream &) = delete;
    AbstractReactiveStream(AbstractReactiveStream &&)                 = delete;
    AbstractReactiveStream &operator=(AbstractReactiveStream &&)      = delete;

  protected:
    /**
     * @brief Constructs the abstract base holding a reference to @p bus.
     *
     * The caller (factory or test harness) guarantees @p bus outlives
     * this facade instance.
     */
    explicit AbstractReactiveStream(vigine::messaging::IMessageBus &bus);

    /**
     * @brief Returns the underlying bus reference for subclass wiring.
     */
    [[nodiscard]] vigine::messaging::IMessageBus &bus() noexcept;

  private:
    vigine::messaging::IMessageBus &_bus;
};

} // namespace vigine::reactivestream
