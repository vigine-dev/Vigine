#include "checkbdshecmetask.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

CheckBDShecmeTask::CheckBDShecmeTask() {}

void CheckBDShecmeTask::contextChanged()
{
    if (!context())
    {
        _dbService = nullptr;

        return;
    }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", vigine::Name("TestDB"), vigine::Property::Exist));
}

vigine::Result CheckBDShecmeTask::run()
{
    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "CheckBDShecmeTask::run: database service is not bound");

    vigine::Result result;

    using namespace vigine::experimental::ecs::postgresql;

    Table table;
    table.setName("Test");
    table.setSchema(Table::Schema::Public);

    Column idColumn, nameColumn, emailColumn;
    idColumn.setName("id");
    idColumn.setType(vigine::experimental::ecs::postgresql::DataType::Integer);
    idColumn.setPrimary(true);

    nameColumn.setName("name");
    nameColumn.setType(vigine::experimental::ecs::postgresql::DataType::Text);

    emailColumn.setName("email");
    emailColumn.setType(vigine::experimental::ecs::postgresql::DataType::Text);

    table.addColumn(idColumn);
    table.addColumn(nameColumn);
    table.addColumn(emailColumn);

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    if (!entity)
        return vigine::Result(vigine::Result::Code::Error,
                              "CheckBDShecmeTask::run: entity 'PostgresBDLocal' not found");

    // Post-#330: legacy @c bindEntity / @c unbindEntity removed; see
    // @c RemoveSomeDataTask for the migration note.
    {
        if (auto *dbConfig = _dbService->databaseConfiguration())
            dbConfig->setTables({table});

        auto checkResultUPtr = _dbService->checkDatabaseScheme();
        if (!checkResultUPtr)
            return vigine::Result(vigine::Result::Code::Error,
                                  "CheckBDShecmeTask::run: checkDatabaseScheme returned a null result");

        result = *checkResultUPtr;
        if (!result.isSuccess())
        {
            std::println("Needed tables don't exist. Let's create them. Error message: {}",
                         result.message());
            auto createResultUPtr = _dbService->createDatabaseScheme();
            if (!createResultUPtr)
                return vigine::Result(vigine::Result::Code::Error,
                                      "CheckBDShecmeTask::run: createDatabaseScheme returned a null result");

            result = *createResultUPtr;
            if (result.isSuccess())
                std::println("Needed Table was created");
        }
    }

    return result;
}
