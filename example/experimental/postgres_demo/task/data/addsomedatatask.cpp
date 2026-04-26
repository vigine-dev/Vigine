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
// this task; it currently returns Success without doing any work
// (the per-row insert loop is commented out below).
vigine::Result AddSomeDataTask::run()
{
    std::println("-- AddSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "AddSomeDataTask::run: database service is not bound");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    if (!entity)
        return vigine::Result(vigine::Result::Code::Error,
                              "AddSomeDataTask::run: entity 'PostgresBDLocal' not found");

    // Post-#330: legacy @c bindEntity / @c unbindEntity removed; the
    // modern @c vigine::service::AbstractService base does not expose
    // them. See @c RemoveSomeDataTask for the migration note.
    {
        // for (int i = 0; i < 100; i += 3)
        //     _dbService->insertData("Test", {vigine::Name("testData_" + std::to_string(i)),
        //                                     vigine::Name("testData_" + std::to_string(i + 1)),
        //                                     vigine::Name("testData_" + std::to_string(i + 2))});
    }

    return vigine::Result();
}
