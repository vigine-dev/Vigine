#include "initbdtask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

InitBDTask::InitBDTask() {}

void InitBDTask::contextChanged()
{
    if (!context())
        {
            _dbService = nullptr;

            return;
        }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", "TestDB", vigine::Property::New));
}

vigine::Result InitBDTask::execute()
{
    std::string serviceName = (_dbService) ? "service was create and it name is " +
                                                 _dbService->id() + "/" + _dbService->name()
                                           : "getting service error";

    std::println("-- InitBDTask::execute(): {}", serviceName);

    auto *entityManager = context()->entityManager();
    vigine::Entity *ent = entityManager->createEntity();

    entityManager->addAlias(ent, "PostgresBDLocal");
    entityManager->addAlias(ent, "PostgresBDLocalCache");

    _dbService->bindEntity(ent);
    {
        _dbService->connectToDb();
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
