#include "checkbdshecmetask.h"

#include <vigine/experimental/ecs/postgresql/impl/column.h>
#include <vigine/experimental/ecs/postgresql/impl/databaseconfiguration.h>
#include <vigine/experimental/ecs/postgresql/impl/table.h>
#include <vigine/result.h>
#include <vigine/impl/service/databaseservice.h>

#include <print>

CheckBDShecmeTask::CheckBDShecmeTask() {}

void CheckBDShecmeTask::setDatabaseService(vigine::DatabaseService *service)
{
    _dbService = service;
}

vigine::Result CheckBDShecmeTask::run()
{
    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "CheckBDShecmeTask::run: database service is not bound");

    using namespace vigine::experimental::ecs::postgresql;

    Table table;
    table.setName("Test");
    table.setSchema(Table::Schema::Public);

    Column idColumn, nameColumn, emailColumn;
    idColumn.setName("id");
    idColumn.setType(DataType::Integer);
    idColumn.setPrimary(true);

    nameColumn.setName("name");
    nameColumn.setType(DataType::Text);

    emailColumn.setName("email");
    emailColumn.setType(DataType::Text);

    table.addColumn(idColumn);
    table.addColumn(nameColumn);
    table.addColumn(emailColumn);

    if (auto *dbConfig = _dbService->databaseConfiguration())
        dbConfig->setTables({table});

    auto checkResult = _dbService->checkDatabaseScheme();
    if (!checkResult)
        return vigine::Result(vigine::Result::Code::Error,
                              "CheckBDShecmeTask::run: checkDatabaseScheme returned a null result");

    vigine::Result result = *checkResult;
    if (!result.isSuccess())
    {
        std::println("Needed tables don't exist. Let's create them. Error message: {}",
                     result.message());

        auto createResult = _dbService->createDatabaseScheme();
        if (!createResult)
            return vigine::Result(vigine::Result::Code::Error,
                                  "CheckBDShecmeTask::run: createDatabaseScheme returned a null result");

        result = *createResult;
        if (result.isSuccess())
            std::println("Needed Table was created");
    }

    return result;
}
