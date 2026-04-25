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

vigine::Result RemoveSomeDataTask::run()
{
    std::println("-- RemoveSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "RemoveSomeDataTask::run: database service is not bound");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    if (!entity)
        return vigine::Result(vigine::Result::Code::Error,
                              "RemoveSomeDataTask::run: entity 'PostgresBDLocal' not found");

    _dbService->bindEntity(entity);
    auto clearResult = _dbService->clearTable("Test");
    _dbService->unbindEntity();

    return clearResult;
}
