#include "vigine/service/databaseservice.h"

#include "vigine/context.h"
#include "vigine/ecs/entity.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/property.h"

#include <iostream>
#include <pqxx/pqxx>

vigine::DatabaseService::DatabaseService(const ServiceName &name) : AbstractService(name) {}

void vigine::DatabaseService::contextChanged()
{
    //_postgress = context()->system("Postgres", "vigineBD", Property::NewIfNotExist);
}

void vigine::DatabaseService::entityBound()
{
    Entity *ent = getBoundEntity();

    if (!_postgress->hasComponents(ent))
        _postgress->addComponentsTo(ent);
}

vigine::Result vigine::DatabaseService::connectToDb()
{
    _postgress->bindEntity(getBoundEntity());
    {
        _postgress->connect("host=localhost port=5432 dbname=vigine user=vigine password=vigine");
        _postgress->exec("SELECT version();");
        auto result = _postgress->result();

        try
            {
                // Підключення до бази
                pqxx::connection conn("host=localhost port=5432 dbname=vigine "
                                      "user=vigine password=vigine");

                pqxx::work txn(conn);
                pqxx::result res = txn.exec("SELECT version();");

                std::cout << "PostgreSQL: " << res[0][0].c_str() << "\n";
                txn.commit();
            }
        catch (const std::exception &e)
            {
                std::cerr << "DB error: " << e.what() << '\n';
            }
    }
    _postgress->unbindEntity();

    return Result();
}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
