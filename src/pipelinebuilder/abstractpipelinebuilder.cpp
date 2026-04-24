#include "vigine/pipelinebuilder/abstractpipelinebuilder.h"

namespace vigine::pipelinebuilder
{

AbstractPipelineBuilder::AbstractPipelineBuilder(
    vigine::messaging::IMessageBus       &bus,
    vigine::core::threading::IThreadManager    &threadManager,
    vigine::channelfactory::IChannelFactory &channelFactory)
    : _bus(bus)
    , _threadManager(threadManager)
    , _channelFactory(channelFactory)
{
}

vigine::messaging::IMessageBus &AbstractPipelineBuilder::bus() noexcept
{
    return _bus;
}

vigine::core::threading::IThreadManager &AbstractPipelineBuilder::threadManager() noexcept
{
    return _threadManager;
}

vigine::channelfactory::IChannelFactory &AbstractPipelineBuilder::channelFactory() noexcept
{
    return _channelFactory;
}

} // namespace vigine::pipelinebuilder
