#include "vigine/channelfactory/abstractchannelfactory.h"

namespace vigine::channelfactory
{

AbstractChannelFactory::AbstractChannelFactory(vigine::messaging::IMessageBus &bus)
    : _bus(bus)
{
}

vigine::messaging::IMessageBus &AbstractChannelFactory::bus() noexcept
{
    return _bus;
}

} // namespace vigine::channelfactory
