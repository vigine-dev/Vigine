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

    // Post-#330: @c DatabaseService derives from the modern
    // @c vigine::service::AbstractService, which does not carry the
    // legacy @c bindEntity / @c unbindEntity surface. The postgres
    // system is now wired through @c DatabaseService::setPostgresSystem
    // by the engine bootstrapper; this demo's full wiring migration is
    // tracked as a follow-up. The previous calls were no-ops on the
    // modern base anyway because @c DatabaseService never overrode the
    // entity-bind hooks.
    auto clearResult = _dbService->clearTable("Test");

    return clearResult;
}
