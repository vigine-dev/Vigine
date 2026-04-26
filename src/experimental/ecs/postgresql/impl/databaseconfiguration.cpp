#include "vigine/experimental/ecs/postgresql/impl/databaseconfiguration.h"

vigine::experimental::ecs::postgresql::DatabaseConfiguration::DatabaseConfiguration() {}

vigine::experimental::ecs::postgresql::DatabaseConfiguration::~DatabaseConfiguration() {}

void vigine::experimental::ecs::postgresql::DatabaseConfiguration::setTables(const std::vector<Table> &tables)
{
    _tables = tables;
}

void vigine::experimental::ecs::postgresql::DatabaseConfiguration::setConnectionData(
    ConnectionDataUPtr &&connectionData)
{
    _connectionData = std::move(connectionData);
}

const std::vector<vigine::experimental::ecs::postgresql::Table> &
vigine::experimental::ecs::postgresql::DatabaseConfiguration::tables() const
{
    return _tables;
}

vigine::experimental::ecs::postgresql::ConnectionData *
vigine::experimental::ecs::postgresql::DatabaseConfiguration::connectionData() const
{
    return _connectionData.get();
}
