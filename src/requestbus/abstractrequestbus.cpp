#include "vigine/requestbus/abstractrequestbus.h"

namespace vigine::requestbus
{

AbstractRequestBus::AbstractRequestBus(vigine::messaging::IMessageBus &bus)
    : _bus(bus)
{
}

vigine::messaging::IMessageBus &AbstractRequestBus::bus() noexcept
{
    return _bus;
}

} // namespace vigine::requestbus
