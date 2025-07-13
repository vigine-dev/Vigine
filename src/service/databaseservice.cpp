#include "vigine/service/databaseservice.h"

#include "vigine/context.h"
#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/ecs/postgresqlsystem.h"
#include "vigine/property.h"

#include <iostream>
#include <pqxx/pqxx>

vigine::DatabaseService::DatabaseService(const ServiceName &name) : AbstractService(name) {}

void vigine::DatabaseService::contextChanged()
{
    _postgressSystem = dynamic_cast<vigine::PostgreSQLSystem *>(
        context()->system("PostgreSQL", "vigineBD", Property::New));
}

bool vigine::DatabaseService::checkTablesExist(const std::vector<Table> &tables) const
{
    bool result = false;
    try
        {
            result = _postgressSystem->checkTableExist(tables[0].name, tables[0].columns);
        }
    catch (const std::exception &e)
        {
            std::cerr << "DB error: " << e.what() << '\n';
        }

    return result;
}

void vigine::DatabaseService::createTables(const std::vector<Table> &tables) const
{
    try
        {
            _postgressSystem->createTable(tables[0].name, tables[0].columns);
        }
    catch (const std::exception &e)
        {
            std::cerr << "DB error: " << e.what() << '\n';
        }
}

std::vector<std::vector<std::string>>
vigine::DatabaseService::readData(const std::string &tableName) const
{
    std::string query = "SELECT * FROM public.\"" + tableName + "\"";
    std::vector<std::vector<std::string>> resultData;
    auto result = _postgressSystem->select(query);

    for (const auto &row : result)
        {
            std::vector<std::string> rowData;

            for (pqxx::row::size_type i = 0; i < row.size(); ++i)
                {
                    if (row[i].is_null())
                        rowData.emplace_back(""); // або "NULL"
                    else
                        rowData.emplace_back(row[i].c_str());
                }

            resultData.push_back(std::move(rowData));
        }

    return resultData;
}

void vigine::DatabaseService::clearTable(const std::string &tableName) const
{
    std::string query = "TRUNCATE TABLE public.\"" + tableName + "\"";

    _postgressSystem->queryRequest(query);
}

void vigine::DatabaseService::insertData(const std::string &tableName,
                                         const std::vector<Column> columnsData)
{
    std::string query = "INSERT INTO public.\"" + tableName + "\"  (col1, col2, col3) VALUES ('" +
                        columnsData.at(0) + "', '" + columnsData.at(1) + "', '" +
                        columnsData.at(2) + "')";

    _postgressSystem->queryRequest(query);
}

void vigine::DatabaseService::entityBound()
{
    Entity *ent = getBoundEntity();

    if (!_postgressSystem->hasComponents(ent))
        _postgressSystem->createComponents(ent);

    _postgressSystem->bindEntity(getBoundEntity());
}

vigine::Result vigine::DatabaseService::connectToDb()
{
    try
        {
            _postgressSystem->setConnectionData({.host       = "localhost",
                                                 .port       = "5432",
                                                 .dbName     = "vigine",
                                                 .dbUserName = "vigine",
                                                 .password   = "vigine"});
            _postgressSystem->connect();
            pqxx::result res = _postgressSystem->select("SELECT version();");

            std::cout << "PostgreSQL: " << res[0][0].c_str() << "\n";
        }
    catch (const std::exception &e)
        {
            std::cerr << "DB error: " << e.what() << '\n';
        }

    return Result();
}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
