#include "databaseconfiguration.h"

vigine::postgresql::DatabaseConfiguration::DatabaseConfiguration() {}

vigine::postgresql::DatabaseConfiguration::~DatabaseConfiguration() {}

void vigine::postgresql::DatabaseConfiguration::setTables(const std::vector<Table> &tables)
{
    _tables = tables;
}

void vigine::postgresql::DatabaseConfiguration::setConnectionData(
    ConnectionDataUPtr &&connectionData)
{
    _connectionData = std::move(connectionData);
}

const std::vector<vigine::postgresql::Table> &
vigine::postgresql::DatabaseConfiguration::tables() const
{
    return _tables;
}

vigine::postgresql::ConnectionData *
vigine::postgresql::DatabaseConfiguration::connectionData() const
{
    return _connectionData.get();
}
