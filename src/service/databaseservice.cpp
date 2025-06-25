#include "vigine/service/databaseservice.h"

#include "vigine/context.h"
#include "vigine/entity.h"
#include "vigine/property.h"

#include <iostream>
#include <pqxx/pqxx>

vigine::DatabaseService::DatabaseService(const ServiceName &name) : AbstractService(name) {}

void vigine::DatabaseService::contextChanged()
{
    _postgress = context()->system("Postgres", "vigineBD", Property::NewIfNotExist);
}

vigine::Entity vigine::DatabaseService::connectToDb()
{

    Entity ent;

    _postgress->bindEntity(ent);

    _postgress->connect("host=localhost port=5432 dbname=vigine user=vigine password=vigine");
    _postgress->exec(ent, "SELECT version();");
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

    return ent;
}

vigine::ServiceId vigine::DatabaseService::id() const { return "Database"; }
