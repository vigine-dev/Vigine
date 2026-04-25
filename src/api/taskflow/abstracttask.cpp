#include <vigine/api/taskflow/abstracttask.h>

#include "vigine/context.h"

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

void AbstractTask::setContext(Context &context)
{
    _context = &context;
    contextChanged();
}

Context *AbstractTask::context() const { return _context; }

void AbstractTask::contextChanged() {}

} // namespace vigine
