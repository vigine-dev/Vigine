#pragma once

#include <memory>

#include "vigine/channelfactory/abstractchannelfactory.h"
#include "vigine/channelfactory/channelkind.h"
#include "vigine/channelfactory/ichannel.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"




namespace vigine::channelfactory
{

/**
 * @brief Concrete final channel-factory facade.
 *
 * @ref DefaultChannelFactory is Level-5 of the five-layer wrapper recipe. It
 * provides the full @ref IChannelFactory implementation on top of
 * @ref AbstractChannelFactory: a channel registry keyed by raw pointer,
 * validated create / close / shutdown lifecycle, and concrete
 * @ref DefaultChannel objects with a mutex-guarded bounded FIFO queue.
 *
 * Callers obtain instances exclusively through @ref createChannelFactory —
 * they never construct this type by name.
 *
 * Thread-safety: @ref create and @ref shutdown are safe to call from any
 * thread concurrently. The channel registry is guarded by a @c std::mutex.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - FF-1: @ref create returns @c std::unique_ptr<IChannel>.
 *   - INV-11: no graph types leak into this header.
 */
class DefaultChannelFactory final : public AbstractChannelFactory
{
  public:
    /**
     * @brief Constructs the channel-factory facade over @p bus.
     *
     * @p bus must outlive this facade instance.
     */
    explicit DefaultChannelFactory(vigine::messaging::IMessageBus &bus);

    ~DefaultChannelFactory() override;

    // IChannelFactory
    [[nodiscard]] std::unique_ptr<IChannel>
        create(ChannelKind                       kind,
               std::size_t                       capacity,
               vigine::payload::PayloadTypeId    expectedTypeId,
               vigine::Result                   *outResult = nullptr) override;

    vigine::Result shutdown() override;

    DefaultChannelFactory(const DefaultChannelFactory &)            = delete;
    DefaultChannelFactory &operator=(const DefaultChannelFactory &) = delete;
    DefaultChannelFactory(DefaultChannelFactory &&)                  = delete;
    DefaultChannelFactory &operator=(DefaultChannelFactory &&)       = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief Factory function — the sole entry point for creating a channel-factory
 *        facade.
 *
 * Returns a @c std::unique_ptr<IChannelFactory> so the caller owns the facade
 * exclusively (FF-1, INV-9). The supplied @p bus must outlive the returned
 * facade.
 */
[[nodiscard]] std::unique_ptr<IChannelFactory>
    createChannelFactory(vigine::messaging::IMessageBus &bus);

} // namespace vigine::channelfactory
