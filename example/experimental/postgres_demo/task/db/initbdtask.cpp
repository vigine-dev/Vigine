#include "initbdtask.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
#include "vigine/experimental/ecs/postgresql/impl/connectiondata.h"
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

vigine::Result InitBDTask::run()
{
    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "InitBDTask::run: database service is not bound");

    auto *entityManager       = context()->entityManager();
    vigine::Entity *ent       = entityManager->createEntity();
    vigine::Entity *entSignal = entityManager->createEntity();

    // Post-#330: legacy @c bindEntity / @c unbindEntity removed; see
    // @c RemoveSomeDataTask for the migration note. The
    // @c databaseConfiguration / @c connectToDb calls below now run
    // against the postgres system the engine bootstrapper attaches via
    // @c DatabaseService::setPostgresSystem.
    {
        auto connectionDataUPtr = std::make_unique<vigine::experimental::ecs::postgresql::ConnectionData>();
        connectionDataUPtr->setHost("localhost");
        connectionDataUPtr->setPort("5432");
        connectionDataUPtr->setDbName(vigine::Name("postgres"));
        connectionDataUPtr->setDbUserName(vigine::Name("postgres"));
        connectionDataUPtr->setPassword(vigine::Password("postgres"));

        if (auto *dbConfig = _dbService->databaseConfiguration())
            dbConfig->setConnectionData(std::move(connectionDataUPtr));

        auto connectResult = _dbService->connectToDb();
        if (connectResult && connectResult->isError())
            std::println("DB connection failed: {}", connectResult->message());
    }

    entityManager->addAlias(ent, "PostgresBDLocal");
    entityManager->addAlias(entSignal, "KeyRleaseEvent");

    return vigine::Result();
}
