#include <vigine/api/taskflow/abstracttask.h>

#include "vigine/context.h"

vigine::AbstractTask::~AbstractTask() {}

vigine::AbstractTask::AbstractTask() {}

void vigine::AbstractTask::setContext(Context *context)
{
    _context = context;
    contextChanged();
}

vigine::Context *vigine::AbstractTask::context() const { return _context; }

void vigine::AbstractTask::contextChanged() {}
