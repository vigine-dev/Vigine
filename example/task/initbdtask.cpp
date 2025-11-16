#include "initbdtask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/ecs/postgresql/connectiondata.h"
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
        context()->service("Database", vigine::Name("TestDB"), vigine::Property::New));
}

vigine::Result InitBDTask::execute()
{
    auto *entityManager = context()->entityManager();
    vigine::Entity *ent = entityManager->createEntity();

    _dbService->bindEntity(ent);
    {
        auto connectionDataUPtr = vigine::postgresql::make_ConnectionDataUPtr();
        connectionDataUPtr->setHost("localhost");
        connectionDataUPtr->setPort("5432");
        connectionDataUPtr->setDbName(vigine::Name("vigine"));
        connectionDataUPtr->setDbUserName(vigine::Name("vigine"));
        connectionDataUPtr->setPassword(vigine::Password("vigine"));

        _dbService->databaseConfiguration()->setConnectionData(std::move(connectionDataUPtr));
        _dbService->connectToDb();
    }
    _dbService->unbindEntity();

    entityManager->addAlias(ent, "PostgresBDLocal");

    return vigine::Result();
}
