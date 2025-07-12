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

void vigine::PostgreSQLSystem::connect()
{
    _boundEntityComponent->connect();
}

pqxx::result vigine::PostgreSQLSystem::select(const std::string &query)
{
    _boundEntityComponent->setQuery(query);
    auto result = _boundEntityComponent->exec();
    _boundEntityComponent->commit();

    return result;
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
