#include "vigine/abstractservice.h"

vigine::AbstractService::~AbstractService() {}

vigine::Name vigine::AbstractService::name() { return _name; }

vigine::AbstractService::AbstractService(const Name &name)
    : ContextHolder(), EntityBindingHost(), _name{name}
{
}
