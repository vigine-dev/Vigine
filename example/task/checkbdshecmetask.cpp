#include "checkbdshecmetask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
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

vigine::Result CheckBDShecmeTask::execute()
{
    vigine::Result result;

    using namespace vigine::postgresql;

    Table table;
    table.setName("Test");
    table.setSchema(Table::Schema::Public);

    Column idColumn, nameColumn, emailColumn;
    idColumn.setName("id");
    idColumn.setType(vigine::postgresql::DataType::Integer);
    idColumn.setPrimary(true);

    nameColumn.setName("name");
    nameColumn.setType(vigine::postgresql::DataType::Text);

    emailColumn.setName("email");
    emailColumn.setType(vigine::postgresql::DataType::Text);

    table.addColumn(idColumn);
    table.addColumn(nameColumn);
    table.addColumn(emailColumn);

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        _dbService->databaseConfiguration()->setTables({table});
        result = *_dbService->checkDatabaseScheme();
        if (!result.isSuccess())
            {
                std::println("Needed tables don't exist. Let's create them. Error message: {}",
                             result.message());
                result = *_dbService->createDatabaseScheme();
                if (result.isSuccess())
                    std::println("Needed Table was created");
            }
    }
    _dbService->unbindEntity();

    return result;
}
