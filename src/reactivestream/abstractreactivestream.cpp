#include "vigine/reactivestream/abstractreactivestream.h"

namespace vigine::reactivestream
{

AbstractReactiveStream::AbstractReactiveStream(vigine::messaging::IMessageBus &bus)
    : _bus(bus)
{
}

vigine::messaging::IMessageBus &AbstractReactiveStream::bus() noexcept
{
    return _bus;
}

} // namespace vigine::reactivestream
