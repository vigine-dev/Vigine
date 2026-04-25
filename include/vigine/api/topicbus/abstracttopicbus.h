#pragma once

#include "vigine/api/topicbus/itopicbus.h"
#include "vigine/messaging/imessagebus.h"

namespace vigine::topicbus
{

/**
 * @brief Stateful abstract base for the topic-bus facade.
 *
 * @ref AbstractTopicBus is Level-4 of the five-layer wrapper recipe
 * (see @c theory_wrapper_creation_recipe.md). It inherits @ref ITopicBus
 * @c public so the topic surface sits at offset zero for zero-cost up-casts,
 * and holds a @ref vigine::messaging::IMessageBus @c & @c protected so the bus
 * substrate is accessible to subclass wiring without leaking the raw bus
 * surface into the public topic-bus API.
 *
 * The class carries state (the bus reference), so it follows the project's
 * @c Abstract naming convention rather than the @c I pure-virtual prefix.
 *
 * Concrete subclasses (for example @ref TopicBus) close the chain by
 * providing the topic registry and the full @ref ITopicBus implementation.
 * Callers never name those types directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref ITopicBus FIRST (mandatory per
 *     5-layer recipe so the facade surface sits at offset zero).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractTopicBus : public ITopicBus
{
  public:
    ~AbstractTopicBus() override = default;

    AbstractTopicBus(const AbstractTopicBus &)            = delete;
    AbstractTopicBus &operator=(const AbstractTopicBus &) = delete;
    AbstractTopicBus(AbstractTopicBus &&)                  = delete;
    AbstractTopicBus &operator=(AbstractTopicBus &&)       = delete;

  protected:
    /**
     * @brief Constructs the abstract base holding a reference to @p bus.
     *
     * The caller (factory or test harness) guarantees @p bus outlives this
     * facade instance.
     */
    explicit AbstractTopicBus(vigine::messaging::IMessageBus &bus);

    /**
     * @brief Returns the underlying bus reference for subclass wiring.
     */
    [[nodiscard]] vigine::messaging::IMessageBus &bus() noexcept;

  private:
    vigine::messaging::IMessageBus &_bus;
};

} // namespace vigine::topicbus
