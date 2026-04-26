#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{

AbstractTask::AbstractTask() = default;

AbstractTask::~AbstractTask() = default;

void AbstractTask::setApi(engine::IEngineToken *api) noexcept
{
    _api = api;
}

engine::IEngineToken *AbstractTask::api() noexcept
{
    return _api;
}

} // namespace vigine
