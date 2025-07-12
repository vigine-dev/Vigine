#include "vigine/ecs/postgresqlsystem.h"

#include "ecs/postgresqlsystem/postgresqlcomponent.h"

vigine::PostgreSQLSystem::PostgreSQLSystem(const SystemName &name) : AbstractSystem(name) {}

vigine::PostgreSQLSystem::~PostgreSQLSystem() {}

bool vigine::PostgreSQLSystem::hasComponents(Entity *entity) const
{
    if (!entity || _entityComponents.empty())
        return false;

    return _entityComponents.contains(entity);
}

void vigine::PostgreSQLSystem::createComponents(Entity *entity)
{
    if (!entity)
        return;

    _entityComponents[entity] = std::make_unique<PostgreSQLComponent>();
}

void vigine::PostgreSQLSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    _entityComponents.erase(entity);
}

void vigine::PostgreSQLSystem::setConnectionData(const ConnectionData &connectionData)
{
    _boundEntityComponent->setHost(connectionData.host);
    _boundEntityComponent->setPort(connectionData.port);
    _boundEntityComponent->setDatabaseName(connectionData.dbName);
    _boundEntityComponent->setDatabaseUser(connectionData.dbUserName);
    _boundEntityComponent->setPassword(connectionData.password);
}

void vigine::PostgreSQLSystem::connect() { _boundEntityComponent->connect(); }

pqxx::result vigine::PostgreSQLSystem::select(const std::string &query)
{
    _boundEntityComponent->setQuery(query);
    auto result = _boundEntityComponent->exec();
    _boundEntityComponent->commit();

    return result;
}

bool vigine::PostgreSQLSystem::checkTableExist(const std::string &tableName,
                                               const std::vector<std::string> tableColumns)
{
    std::string query = "SELECT EXISTS (SELECT 1 FROM pg_catalog.pg_tables WHERE schemaname = "
                        "'public' AND tablename = '" +
                        tableName + "')";

    _boundEntityComponent->setQuery(query);
    auto result = _boundEntityComponent->exec();
    _boundEntityComponent->commit();
    auto resultAsBool = result[0][0].as<bool>();

    if (resultAsBool)
        {
            std::string cols;
            for (size_t i = 0; i < tableColumns.size(); ++i)
                {
                    cols += "'" + tableColumns[i] + "'";
                    if (i + 1 < tableColumns.size())
                        cols += ",";
                }

            std::string query = "SELECT COUNT(*) FROM information_schema.columns "
                                "WHERE table_schema = 'public' "
                                "AND table_name = '" +
                                tableName +
                                "' "
                                "AND column_name IN (" +
                                cols + ")";

            _boundEntityComponent->setQuery(query);
            result = _boundEntityComponent->exec();
            _boundEntityComponent->commit();
            resultAsBool = result[0][0].as<int>() == tableColumns.size();
        }

    return resultAsBool;
}

void vigine::PostgreSQLSystem::createTable(const std::string &tableName,
                                           const std::vector<std::string> tableColumns)
{
    std::string cols;
    for (size_t i = 0; i < tableColumns.size(); ++i)
        {
            cols += "" + tableColumns[i] + " TEXT";
            if (i + 1 < tableColumns.size())
                cols += ",";
        }

    std::string query = "CREATE TABLE IF NOT EXISTS public.\"" + tableName + "\" (" + cols + ")";

    _boundEntityComponent->setQuery(query);
    _boundEntityComponent->exec();
    _boundEntityComponent->commit();
}

void vigine::PostgreSQLSystem::entityBound()
{
    auto boundEntity      = getBoundEntity();
    _boundEntityComponent = nullptr;

    if (_entityComponents.contains(boundEntity))
        _boundEntityComponent = _entityComponents.at(boundEntity).get();
}

void vigine::PostgreSQLSystem::entityUnbound() { _boundEntityComponent = nullptr; }

vigine::SystemId vigine::PostgreSQLSystem::id() const { return "PostgreSQL"; }
