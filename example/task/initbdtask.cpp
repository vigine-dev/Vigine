#include "initbdtask.h"

#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

InitBDTask::InitBDTask() {}

void InitBDTask::contextChanged()
{
    if (!context())
        {
            _dbService = nullptr;

            return;
        }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", "TestDB", vigine::Property::New));
}

vigine::Result InitBDTask::execute()
{
    std::string serviceName = (_dbService) ? "service was create and it name is " +
                                                 _dbService->id() + "/" + _dbService->name()
                                           : "getting service error";

    std::println("-- InitBDTask::execute(): {}", serviceName);

    _dbService->connectToDb();

    return vigine::Result();
}
