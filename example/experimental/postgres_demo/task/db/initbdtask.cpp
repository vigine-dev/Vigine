#include "initbdtask.h"

#include "vigine/experimental/ecs/postgresql/impl/connectiondata.h"
#include <vigine/result.h>
#include <vigine/impl/service/databaseservice.h>

#include <print>

InitBDTask::InitBDTask() {}

void InitBDTask::setDatabaseService(vigine::DatabaseService *service)
{
    _dbService = service;
}

vigine::Result InitBDTask::run()
{
    std::println("-- InitBDTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "InitBDTask::run: database service is not bound");

    // Configure the connection string the postgres system will dial out
    // to. The values match the legacy demo's local-postgres assumption.
    auto connectionData = std::make_unique<vigine::experimental::ecs::postgresql::ConnectionData>();
    connectionData->setHost("localhost");
    connectionData->setPort("5432");
    connectionData->setDbName(vigine::Name("postgres"));
    connectionData->setDbUserName(vigine::Name("postgres"));
    connectionData->setPassword(vigine::Password("postgres"));

    if (auto *dbConfig = _dbService->databaseConfiguration())
        dbConfig->setConnectionData(std::move(connectionData));

    auto connectResult = _dbService->connectToDb();
    if (connectResult && connectResult->isError())
    {
        std::println("DB connection failed: {}", connectResult->message());
        return *connectResult;
    }

    return vigine::Result();
}
