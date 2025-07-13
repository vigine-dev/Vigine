#include "addsomedatatask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

AddSomeDataTask::AddSomeDataTask() {}

void AddSomeDataTask::contextChanged()
{
    if (!context())
        {
            _dbService = nullptr;

            return;
        }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", "TestDB", vigine::Property::Exist));
}

vigine::Result AddSomeDataTask::execute()
{
    std::println("-- AddSomeDataTask::execute()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        for (int i = 0; i < 100; i += 3)
            _dbService->insertData("Test", {"testData_" + std::to_string(i),
                                            "testData_" + std::to_string(i + 1),
                                            "testData_" + std::to_string(i + 2)});
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
