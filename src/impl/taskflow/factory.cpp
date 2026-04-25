#include "vigine/api/taskflow/factory.h"

#include <memory>

#include "taskflow/defaulttaskflow.h"

namespace vigine::taskflow
{

std::unique_ptr<ITaskFlow> createTaskFlow()
{
    // The factory constructs the default concrete closer over
    // AbstractTaskFlow. The internal task orchestrator is allocated
    // eagerly by the base class constructor, which also auto-provisions
    // the default task per UD-3, so the returned flow is immediately
    // ready to answer @ref ITaskFlow::current with a valid id.
    return std::make_unique<DefaultTaskFlow>();
}

} // namespace vigine::taskflow
