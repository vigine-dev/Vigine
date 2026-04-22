#include "vigine/topicbus/abstracttopicbus.h"

namespace vigine::topicbus
{

AbstractTopicBus::AbstractTopicBus(vigine::messaging::IMessageBus &bus)
    : _bus(bus)
{
}

vigine::messaging::IMessageBus &AbstractTopicBus::bus() noexcept
{
    return _bus;
}

} // namespace vigine::topicbus
