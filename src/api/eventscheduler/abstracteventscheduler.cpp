#include "vigine/api/eventscheduler/abstracteventscheduler.h"

#include <utility>

namespace vigine::eventscheduler
{

AbstractEventScheduler::AbstractEventScheduler(
    vigine::messaging::BusConfig      config,
    vigine::core::threading::IThreadManager &threadManager)
    : vigine::messaging::AbstractMessageBus{std::move(config), threadManager}
{
}

} // namespace vigine::eventscheduler
