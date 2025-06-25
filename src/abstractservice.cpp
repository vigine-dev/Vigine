#include "vigine/abstractservice.h"

vigine::AbstractService::~AbstractService() {}

vigine::ServiceName vigine::AbstractService::name() { return _name; }

vigine::AbstractService::AbstractService(const ServiceName &name) : ContextHolder(), _name{name} {}
