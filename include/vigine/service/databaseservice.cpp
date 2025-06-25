#include "databaseservice.h"

vigine::DatabaseService::DatabaseService(const ServiceName &name)
    : AbstractService(name) {}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
