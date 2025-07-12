#include "contextholder.h"

#include "vigine/context.h"

vigine::ContextHolder::~ContextHolder() {}

void vigine::ContextHolder::setContext(Context *context)
{
    _context = context;
    contextChanged();
}

vigine::ContextHolder::ContextHolder() {}

void vigine::ContextHolder::contextChanged() {}

vigine::Context *vigine::ContextHolder::context() const { return _context; }
