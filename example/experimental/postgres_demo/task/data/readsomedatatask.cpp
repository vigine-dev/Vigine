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

// COPILOT_TODO: Перевіряти entity/_dbService і повертати помилку, якщо DatabaseService::readData()
// лишається недописаним; зараз сценарій маскує неготовий код як success.
vigine::Result ReadSomeDataTask::run()
{
    std::println("-- ReadSomeDataTask::run()");

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
