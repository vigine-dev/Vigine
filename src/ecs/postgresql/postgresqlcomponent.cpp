#include "postgresqlcomponent.h"

#include "vigine/ecs/postgresql/query/querybuilder.h"

#include <print>
#include <ranges>
#include <vector>

vigine::postgresql::PostgreSQLComponent::PostgreSQLComponent()
    : _dbConfig{make_DatabaseConfigurationUPtr()}
{
}

vigine::postgresql::PostgreSQLComponent::~PostgreSQLComponent() {}

pqxx::result
vigine::postgresql::PostgreSQLComponent::exec_raw(const query::QueryBuilder &queryBuilder)
{
    setQuery(queryBuilder);
    _work       = std::make_unique<pqxx::work>(*_connection);

    auto result = _work->exec(_query);
    commit();

    return result;
}

vigine::postgresql::PostgreSQLResultUPtr
vigine::postgresql::PostgreSQLComponent::exec(const query::QueryBuilder &queryBuilder)
{
    setQuery(queryBuilder);
    auto result = exec();

    return std::move(result);
}

vigine::postgresql::PostgreSQLResultUPtr vigine::postgresql::PostgreSQLComponent::connect()
{
    std::string connectionString = "host=" + _dbConfig->connectionData()->host() + " " +
                                   "dbname=" + _dbConfig->connectionData()->dbName().str() + " " +
                                   "user=" + _dbConfig->connectionData()->dbUserName().str() + " " +
                                   "password=" + _dbConfig->connectionData()->password().str();

    _connection = std::make_unique<pqxx::connection>(connectionString);

    initTypeConverter();

    if (!_pgTypeConverter)
        return make_PostgreSQLResultUPtr(Result::Code::Error,
                                         "postgreSQL type converter should be initialize");

    return make_PostgreSQLResultUPtr();
}

vigine::postgresql::PostgreSQLResultUPtr vigine::postgresql::PostgreSQLComponent::exec()
{
    _work           = std::make_unique<pqxx::work>(*_connection);
    auto pgxxResult = _work->exec(_query);
    commit();

    auto pgResult = make_PostgreSQLResultUPtr(pgxxResult, pgTypeConverter());

    return std::move(pgResult);
}

void vigine::postgresql::PostgreSQLComponent::commit() { _work->commit(); }

void vigine::postgresql::PostgreSQLComponent::setQuery(const std::string &query) { _query = query; }

void vigine::postgresql::PostgreSQLComponent::setHost(const std::string &host) { _host = host; }

void vigine::postgresql::PostgreSQLComponent::setPort(const std::string &port) { _port = port; }

void vigine::postgresql::PostgreSQLComponent::setDatabaseName(const std::string &dbName)
{
    _dbName = dbName;
}

void vigine::postgresql::PostgreSQLComponent::setDatabaseUser(const std::string &dbUserName)
{
    _dbUserName = dbUserName;
}

void vigine::postgresql::PostgreSQLComponent::setPassword(const std::string &password)
{
    _password = password;
}

void vigine::postgresql::PostgreSQLComponent::setPgTypeConverter(
    PostgreSQLTypeConverterUPtr converter)
{
    _pgTypeConverter = std::move(converter);
}

vigine::postgresql::PostgreSQLTypeConverterPtr
vigine::postgresql::PostgreSQLComponent::pgTypeConverter() const
{
    return _pgTypeConverter.get();
}

void vigine::postgresql::PostgreSQLComponent::initTypeConverter() {}

vigine::postgresql::DatabaseConfiguration *
vigine::postgresql::PostgreSQLComponent::dbConfiguration() const
{
    return _dbConfig.get();
}
