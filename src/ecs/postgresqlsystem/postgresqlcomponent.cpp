#include "postgresqlcomponent.h"

vigine::PostgreSQLComponent::PostgreSQLComponent() {}

vigine::PostgreSQLComponent::~PostgreSQLComponent() {}

void vigine::PostgreSQLComponent::connect()
{
    std::string connectionString = "host=" + _host + " " + "dbname=" + _dbName + " " +
                                   "user=" + _dbUserName + " " + "password=" + _password;

    _connection = std::make_unique<pqxx::connection>(connectionString);
}

pqxx::result vigine::PostgreSQLComponent::exec()
{
    _work = std::make_unique<pqxx::work>(*_connection);
    return _work->exec(_query);
}

void vigine::PostgreSQLComponent::commit() { _work->commit(); }

void vigine::PostgreSQLComponent::setQuery(const std::string &query) { _query = query; }

void vigine::PostgreSQLComponent::setHost(const std::string &host) { _host = host; }

void vigine::PostgreSQLComponent::setPort(const std::string &port) { _port = port; }

void vigine::PostgreSQLComponent::setDatabaseName(const std::string &dbName) { _dbName = dbName; }

void vigine::PostgreSQLComponent::setDatabaseUser(const std::string &dbUserName)
{
    _dbUserName = dbUserName;
}

void vigine::PostgreSQLComponent::setPassword(const std::string &password) { _password = password; }
