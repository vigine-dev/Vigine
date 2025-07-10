#include "vigine/ecs/postgresqlsystem.h"

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

    _entityComponents[entity] = std::make_unique<PostgreSQLSystemComponents>();
}

void vigine::PostgreSQLSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    _entityComponents.erase(entity);
}

vigine::SystemId vigine::PostgreSQLSystem::id() const { return "PostgreSQL"; }
