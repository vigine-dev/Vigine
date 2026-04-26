#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{

AbstractTask::AbstractTask() = default;

AbstractTask::~AbstractTask() = default;

void AbstractTask::setApiToken(engine::IEngineToken *apiToken) noexcept
{
    _apiToken = apiToken;
}

engine::IEngineToken *AbstractTask::apiToken() noexcept
{
    return _apiToken;
}

} // namespace vigine
