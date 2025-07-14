#include "vigine/service/databaseservice.h"

#include "vigine/context.h"
#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/ecs/postgresql/postgresqlsystem.h"
#include "vigine/property.h"

#include <iostream>
#include <pqxx/pqxx>

 // TODO: refactor. Check unbound entity

vigine::DatabaseService::DatabaseService(const Name &name) : AbstractService(name) {}

void vigine::DatabaseService::contextChanged()
{
    _postgressSystem = dynamic_cast<vigine::postgresql::PostgreSQLSystem *>(
        context()->system("PostgreSQL", "vigineBD", Property::New));
}

vigine::ResultUPtr vigine::DatabaseService::checkDatabaseScheme()
{
    return _postgressSystem->checkTablesScheme();
}

vigine::ResultUPtr vigine::DatabaseService::createDatabaseScheme()
{
    vigine::ResultUPtr result = make_ResultUPtr();

    return result;
}

vigine::postgresql::DatabaseConfiguration *vigine::DatabaseService::databaseConfiguration()
{
    return _postgressSystem->dbConfiguration();
}

 // TODO: reimplement or remove
std::vector<std::vector<std::string>>
vigine::DatabaseService::readData(const std::string &tableName) const
{
    std::string query = "SELECT * FROM public.\"" + tableName + "\"";
    std::vector<std::vector<std::string>> resultData;
    // auto result = _postgressSystem->select(query);

    // for (const auto &row : result)
    //     {
    //         std::vector<std::string> rowData;

    //         for (pqxx::row::size_type i = 0; i < row.size(); ++i)
    //             {
    //                 if (row[i].is_null())
    //                     rowData.emplace_back(""); // або "NULL"
    //                 else
    //                     rowData.emplace_back(row[i].c_str());
    //             }

    //         resultData.push_back(std::move(rowData));
    //     }

    return resultData;
}

void vigine::DatabaseService::clearTable(const std::string &tableName) const
{
    std::string query = "TRUNCATE TABLE public.\"" + tableName + "\"";

    _postgressSystem->queryRequest(query);
}

 // TODO: remove or update
void vigine::DatabaseService::writeData(const std::string &tableName,
                                        const std::vector<postgresql::Column> columnsData)
{
    std::string query = "INSERT INTO public.\"" + tableName + "\"  (col1, col2, col3) VALUES ('" +
                        columnsData.at(0).name() + "', '" + columnsData.at(1).name() + "', '" +
                        columnsData.at(2).name() + "')";

    _postgressSystem->queryRequest(query);
}

void vigine::DatabaseService::entityBound()
{
    Entity *ent = getBoundEntity();

    if (!_postgressSystem->hasComponents(ent))
        _postgressSystem->createComponents(ent);

    _postgressSystem->bindEntity(getBoundEntity());
}

vigine::ResultUPtr vigine::DatabaseService::connectToDb()
{
    ResultUPtr result;

    try
        {
            result = _postgressSystem->connect();
        }
    catch (const std::exception &e)
        {
            std::cerr << "DB error: " << e.what() << '\n';
            result = make_ResultUPtr(Result::Code::Error, e.what());
        }

    return result;
}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
