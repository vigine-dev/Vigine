#include "removesomedatatask.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

RemoveSomeDataTask::RemoveSomeDataTask() {}

void RemoveSomeDataTask::contextChanged()
{
    if (!context())
    {
        _dbService = nullptr;

        return;
    }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", vigine::Name("TestDB"), vigine::Property::Exist));
}

// COPILOT_TODO: Validate entity/_dbService and provide an error-return
// path from clearTable/queryRequest; otherwise database failures here
// go unnoticed.
vigine::Result RemoveSomeDataTask::run()
{
    std::println("-- RemoveSomeDataTask::run()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        _dbService->clearTable("Test");
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
