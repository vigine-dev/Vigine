#pragma once

#include <memory>

#include "vigine/api/channelfactory/abstractchannelfactory.h"
#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::channelfactory
{

/**
 * @brief Concrete final channel-factory facade.
 *
 * @ref ChannelFactory is Level-5 of the five-layer wrapper recipe. It
 * provides the full @ref IChannelFactory implementation on top of
 * @ref AbstractChannelFactory: a channel registry keyed by raw pointer,
 * validated create / close / shutdown lifecycle, and concrete
 * channel objects with a mutex-guarded bounded FIFO queue.
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
class ChannelFactory final : public AbstractChannelFactory
{
  public:
    /**
     * @brief Constructs the channel-factory facade over @p bus.
     *
     * @p bus must outlive this facade instance.
     */
    explicit ChannelFactory(vigine::messaging::IMessageBus &bus);

    ~ChannelFactory() override;

    // IChannelFactory
    [[nodiscard]] std::unique_ptr<IChannel>
        create(ChannelKind                       kind,
               std::size_t                       capacity,
               vigine::payload::PayloadTypeId    expectedTypeId,
               vigine::Result                   *outResult = nullptr) override;

    vigine::Result shutdown() override;

    ChannelFactory(const ChannelFactory &)            = delete;
    ChannelFactory &operator=(const ChannelFactory &) = delete;
    ChannelFactory(ChannelFactory &&)                  = delete;
    ChannelFactory &operator=(ChannelFactory &&)       = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::channelfactory
