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

void vigine::DatabaseService::entityBound()
{
    Entity *ent = getBoundEntity();

    if (!_postgressSystem->hasComponents(ent))
        _postgressSystem->createComponents(ent);
}

vigine::Result vigine::DatabaseService::connectToDb()
{
    _postgressSystem->bindEntity(getBoundEntity());
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
    }
    _postgressSystem->unbindEntity();

    return Result();
}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
