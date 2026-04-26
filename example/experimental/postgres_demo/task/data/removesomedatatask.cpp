#include "removesomedatatask.h"

#include <vigine/result.h>
#include <vigine/impl/service/databaseservice.h>

#include <print>

RemoveSomeDataTask::RemoveSomeDataTask() {}

void RemoveSomeDataTask::setDatabaseService(vigine::DatabaseService *service)
{
    _dbService = service;
}

vigine::Result RemoveSomeDataTask::run()
{
    std::println("-- RemoveSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "RemoveSomeDataTask::run: database service is not bound");

    return _dbService->clearTable("Test");
}
