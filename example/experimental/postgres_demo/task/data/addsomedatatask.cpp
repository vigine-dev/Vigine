#include "addsomedatatask.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
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
        context()->service("Database", vigine::Name("TestDB"), vigine::Property::Exist));
}

// COPILOT_TODO: Either implement a real insert scenario or remove
// this task; it currently returns Success without doing any work and
// without validating entity/_dbService.
vigine::Result AddSomeDataTask::run()
{
    std::println("-- AddSomeDataTask::run()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        // for (int i = 0; i < 100; i += 3)
        //     _dbService->insertData("Test", {vigine::Name("testData_" + std::to_string(i)),
        //                                     vigine::Name("testData_" + std::to_string(i + 1)),
        //                                     vigine::Name("testData_" + std::to_string(i + 2))});
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
