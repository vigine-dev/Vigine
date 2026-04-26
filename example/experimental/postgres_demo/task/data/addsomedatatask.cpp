#include "addsomedatatask.h"

#include <vigine/result.h>
#include <vigine/impl/service/databaseservice.h>

#include <print>

AddSomeDataTask::AddSomeDataTask() {}

void AddSomeDataTask::setDatabaseService(vigine::DatabaseService *service)
{
    _dbService = service;
}

// The legacy demo's per-row insert loop is intentionally left commented
// out: the postgres CRUD path is exercised by the Read / Remove tasks
// below, and the schema-bootstrap step in CheckBDShecme already
// validates that the table exists. Wiring real inserts is a separate
// follow-up tracked alongside the row-fetch refactor on
// `DatabaseService::readData`.
vigine::Result AddSomeDataTask::run()
{
    std::println("-- AddSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "AddSomeDataTask::run: database service is not bound");

    return vigine::Result();
}
