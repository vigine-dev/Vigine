#include "readsomedatatask.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
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
        context()->service("Database", "TestDB", vigine::Property::Exist));
}

vigine::Result ReadSomeDataTask::execute()
{
    std::println("-- ReadSomeDataTask::execute()");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("PostgresBDLocal");

    _dbService->bindEntity(entity);
    {
        std::vector<std::vector<std::string>> result = _dbService->readData("Test");
        for (int i = 0; i < result.size(); ++i)
            {
                auto item          = result[i];
                std::string rowStr = "Row (" + std::to_string(i + 1) + ") data: ";
                for (auto cell : item)
                    rowStr += cell + " ";

                std::println("{}", rowStr);
            }
    }
    _dbService->unbindEntity();

    return vigine::Result();
}
