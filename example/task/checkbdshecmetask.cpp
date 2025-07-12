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
        context()->service("Database", "TestDB", vigine::Property::Exist));
}

vigine::Result CheckBDShecmeTask::execute()
{
    std::println("-- CheckBDShecmeTask::execute()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        std::vector<vigine::Table> tables;
        vigine::Table testTable{
            .name = "Test", .columns = {"col1", "col2", "col3"}
        };

        tables.push_back(testTable);

        if (_dbService->checkTablesExist(tables))
            {
                std::println("Needed Tables exist");
            }
        else
            {
                std::println("Needed tables don't exist. Let's create them.");
                _dbService->createTables(tables);
                if (_dbService->checkTablesExist(tables))
                    {
                        std::println("Needed Table was created");
                    }
            }
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
