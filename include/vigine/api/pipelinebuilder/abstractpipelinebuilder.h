#pragma once

#include "vigine/api/channelfactory/ichannelfactory.h"
#include "vigine/api/pipelinebuilder/ipipelinebuilder.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/messaging/imessagebus.h"

namespace vigine::pipelinebuilder
{

/**
 * @brief Stateful abstract base for the pipeline-builder facade.
 *
 * @ref AbstractPipelineBuilder is Level-4 of the five-layer wrapper
 * recipe.  It inherits @ref IPipelineBuilder @c public so the builder
 * surface sits at offset zero for zero-cost up-casts, and holds
 * references to the underlying @ref vigine::messaging::IMessageBus,
 * @ref vigine::core::threading::IThreadManager, and
 * @ref vigine::channelfactory::IChannelFactory @c protected so subclass
 * wiring can reach them without exposing the raw surfaces in the public
 * builder API.
 *
 * The class carries state (three references), so it follows the project's
 * @c Abstract naming convention rather than the @c I pure-virtual prefix.
 *
 * Concrete subclasses (e.g. @ref PipelineBuilder) close the
 * chain by providing the stage vector, the drain-channel lifecycle, and
 * the full @ref IPipelineBuilder implementation.  Callers never name
 * those types directly.
 *
 * Invariants:
 *   - INV-10: @c Abstract prefix for an abstract class with state.
 *   - INV-11: no graph types leak through this header.
 *   - Inheritance order: @c public @ref IPipelineBuilder FIRST (mandatory
 *     per 5-layer recipe so the facade surface sits at offset zero).
 *   - All data members are @c private (strict encapsulation).
 */
class AbstractPipelineBuilder : public IPipelineBuilder
{
  public:
    ~AbstractPipelineBuilder() override = default;

    AbstractPipelineBuilder(const AbstractPipelineBuilder &)            = delete;
    AbstractPipelineBuilder &operator=(const AbstractPipelineBuilder &) = delete;
    AbstractPipelineBuilder(AbstractPipelineBuilder &&)                 = delete;
    AbstractPipelineBuilder &operator=(AbstractPipelineBuilder &&)      = delete;

  protected:
    /**
     * @brief Constructs the abstract base holding references to @p bus,
     *        @p threadManager, and @p channelFactory.
     *
     * The caller (factory or test harness) guarantees all three
     * references outlive this facade instance.
     */
    AbstractPipelineBuilder(vigine::messaging::IMessageBus       &bus,
                            vigine::core::threading::IThreadManager    &threadManager,
                            vigine::channelfactory::IChannelFactory &channelFactory);

    /**
     * @brief Returns the underlying bus reference for subclass wiring.
     */
    [[nodiscard]] vigine::messaging::IMessageBus &bus() noexcept;

    /**
     * @brief Returns the thread manager reference for subclass wiring.
     */
    [[nodiscard]] vigine::core::threading::IThreadManager &threadManager() noexcept;

    /**
     * @brief Returns the channel factory reference for subclass wiring.
     */
    [[nodiscard]] vigine::channelfactory::IChannelFactory &channelFactory() noexcept;

  private:
    vigine::messaging::IMessageBus       &_bus;
    vigine::core::threading::IThreadManager    &_threadManager;
    vigine::channelfactory::IChannelFactory &_channelFactory;
};

} // namespace vigine::pipelinebuilder
