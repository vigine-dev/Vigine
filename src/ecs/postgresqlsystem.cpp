#include "vigine/ecs/postgresqlsystem.h"

vigine::PostgreSQLSystem::PostgreSQLSystem(const SystemName &name) : AbstractSystem(name) {}

vigine::PostgreSQLSystem::~PostgreSQLSystem() {}

vigine::SystemId vigine::PostgreSQLSystem::id() const { return "PostgreSQL"; }
