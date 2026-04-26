#pragma once

#include <memory>

#include "vigine/api/channelfactory/ichannelfactory.h"

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::channelfactory
{

/**
 * @brief Factory function -- the sole public entry point for creating
 *        a channel-factory facade.
 *
 * Returns a @c std::unique_ptr<IChannelFactory> so the caller owns the
 * facade exclusively (FF-1, INV-9). The supplied @p bus must outlive
 * the returned facade; the facade keeps a non-owning reference to it.
 *
 * This header is factory-only on purpose: it forward-declares the
 * @ref vigine::messaging::IMessageBus argument type and pulls only
 * @c ichannelfactory.h, so callers that want just the factory do not
 * pull the concrete @c ChannelFactory class body into their
 * translation units. Callers that need to name the concrete type can
 * still include @c vigine/impl/channelfactory/channelfactory.h directly.
 *
 * Invariants:
 *   - INV-9:  @c createChannelFactory returns @c std::unique_ptr<IChannelFactory>.
 *   - INV-11: no graph types appear here.
 */
[[nodiscard]] std::unique_ptr<IChannelFactory>
    createChannelFactory(vigine::messaging::IMessageBus &bus);

} // namespace vigine::channelfactory
