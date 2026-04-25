#pragma once

#include <memory>

#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::channelfactory
{

/**
 * @brief Pure-virtual facade-level factory producing @ref IChannel handles.
 *
 * @ref IChannelFactory is the Level-2 facade over
 * @ref vigine::messaging::IMessageBus for CSP-style bounded channels
 * (plan_18, R.3.3.1.4). It encapsulates three operations:
 *
 *   - @ref create   -- allocates a new channel of the given kind and capacity,
 *                       validates the config edge cases, and returns a
 *                       @c std::unique_ptr<IChannel>.
 *   - @ref shutdown -- cancels all open channels (closes each one) and
 *                       rejects subsequent @ref create calls.
 *
 * Config edge cases (enforced by @ref create):
 *   - @ref ChannelKind::Bounded  with capacity < 1 returns error.
 *   - @ref ChannelKind::Unbounded with capacity != 0 returns error.
 *
 * Ownership:
 *   - @ref create returns a @c std::unique_ptr<IChannel> (FF-1, INV-9).
 *     The caller owns the channel; destroying it without first calling
 *     @ref IChannel::close discards any queued payloads silently.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-9: @ref create returns @c std::unique_ptr.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IChannelFactory
{
  public:
    virtual ~IChannelFactory() = default;

    /**
     * @brief Creates a new channel of the given @p kind with the given
     *        @p capacity and expected payload type @p expectedTypeId.
     *
     * Returns a non-null @c std::unique_ptr<IChannel> on success. On invalid
     * config (capacity == 0 with Bounded, or capacity != 0 with Unbounded)
     * returns a null @c std::unique_ptr and sets @p outResult to an error
     * @ref vigine::Result. If @p outResult is null the error detail is
     * silently discarded.
     *
     * Subsequent @ref create calls after @ref shutdown return null.
     */
    [[nodiscard]] virtual std::unique_ptr<IChannel>
        create(ChannelKind                       kind,
               std::size_t                       capacity,
               vigine::payload::PayloadTypeId    expectedTypeId,
               vigine::Result                   *outResult = nullptr) = 0;

    /**
     * @brief Shuts down the factory.
     *
     * Closes every channel that was created through this factory and still
     * exists, then rejects subsequent @ref create calls. Idempotent.
     */
    virtual vigine::Result shutdown() = 0;

    IChannelFactory(const IChannelFactory &)            = delete;
    IChannelFactory &operator=(const IChannelFactory &) = delete;
    IChannelFactory(IChannelFactory &&)                 = delete;
    IChannelFactory &operator=(IChannelFactory &&)      = delete;

  protected:
    IChannelFactory() = default;
};

} // namespace vigine::channelfactory
