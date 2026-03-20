#include "removesomedatatask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
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

// COPILOT_TODO: Перевіряти entity/_dbService і мати шлях повернення помилки з
// clearTable/queryRequest, інакше збої БД тут залишаться непоміченими.
vigine::Result RemoveSomeDataTask::execute()
{
    std::println("-- RemoveSomeDataTask::execute()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        _dbService->clearTable("Test");
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
