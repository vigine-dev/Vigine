#include "readsomedatatask.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <print>

ReadSomeDataTask::ReadSomeDataTask() {}

void ReadSomeDataTask::contextChanged()
{
    if (!context())
    {
        _dbService = nullptr;

        return;
    }

    _dbService = dynamic_cast<vigine::DatabaseService *>(
        context()->service("Database", vigine::Name("TestDB"), vigine::Property::Exist));
}

// COPILOT_TODO: @c DatabaseService::readData() still returns an empty
// vector unconditionally; the loop below masks that as success. Real
// row-fetch wiring is a separate follow-up.
vigine::Result ReadSomeDataTask::run()
{
    std::println("-- ReadSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "ReadSomeDataTask::run: database service is not bound");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    if (!entity)
        return vigine::Result(vigine::Result::Code::Error,
                              "ReadSomeDataTask::run: entity 'PostgresBDLocal' not found");

    // Post-#330: legacy @c bindEntity / @c unbindEntity removed; see
    // @c RemoveSomeDataTask for the migration note.
    {
        std::vector<std::vector<std::string>> result = _dbService->readData("Test");
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            auto item          = result[i];
            std::string rowStr = "Row (" + std::to_string(i + 1) + ") data: ";
            for (auto cell : item)
                rowStr += cell + " ";

            std::println("{}", rowStr);
        }
    }

    return vigine::Result();
}
