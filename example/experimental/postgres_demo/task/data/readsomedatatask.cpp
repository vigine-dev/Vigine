#include "readsomedatatask.h"

#include <vigine/result.h>
#include <vigine/service/databaseservice.h>

#include <print>
#include <string>
#include <vector>

ReadSomeDataTask::ReadSomeDataTask() {}

void ReadSomeDataTask::setDatabaseService(vigine::DatabaseService *service)
{
    _dbService = service;
}

// `DatabaseService::readData` currently returns an empty vector
// unconditionally; the loop below is correct against that contract --
// it just produces no output when no rows are fetched. The full
// row-fetch wiring is tracked alongside the parameterised-query
// refactor on `DatabaseService::readData`.
vigine::Result ReadSomeDataTask::run()
{
    std::println("-- ReadSomeDataTask::run()");

    if (!_dbService)
        return vigine::Result(vigine::Result::Code::Error,
                              "ReadSomeDataTask::run: database service is not bound");

    const std::vector<std::vector<std::string>> rows = _dbService->readData("Test");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        std::string rowStr = "Row (" + std::to_string(i + 1) + ") data: ";
        for (const auto &cell : rows[i])
            rowStr += cell + " ";

        std::println("{}", rowStr);
    }

    return vigine::Result();
}
