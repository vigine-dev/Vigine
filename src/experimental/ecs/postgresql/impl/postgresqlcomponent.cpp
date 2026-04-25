#include "postgresqlcomponent.h"

#include "vigine/experimental/ecs/postgresql/impl/query/querybuilder.h"

#include <stdexcept>

namespace
{

std::string buildConnectionString(const vigine::experimental::ecs::postgresql::ConnectionData &connectionData)
{
    std::string connectionString = "host=" + connectionData.host() + " ";

    if (!connectionData.port().empty())
        connectionString += "port=" + connectionData.port() + " ";

    connectionString += "dbname=" + connectionData.dbName().str() + " ";
    connectionString += "user=" + connectionData.dbUserName().str();

    if (!connectionData.password().str().empty())
        connectionString += " password=" + connectionData.password().str();

    return connectionString;
}

void validateQueryExecution(const pqxx::connection *connection, const std::string &query)
{
    if (!connection)
        throw std::runtime_error("PostgreSQL connection is not initialized");

    if (!connection->is_open())
        throw std::runtime_error("PostgreSQL connection is closed");

    if (query.empty())
        throw std::runtime_error("PostgreSQL query is empty");
}

} // namespace

vigine::experimental::ecs::postgresql::PostgreSQLComponent::PostgreSQLComponent()
    : _dbConfig{std::make_unique<DatabaseConfiguration>()}
{
}

vigine::experimental::ecs::postgresql::PostgreSQLComponent::~PostgreSQLComponent() {}

pqxx::result
vigine::experimental::ecs::postgresql::PostgreSQLComponent::exec_raw(const query::QueryBuilder &queryBuilder)
{
    setQuery(queryBuilder);
    validateQueryExecution(_connection.get(), _query);

    try
    {
        _work       = std::make_unique<pqxx::work>(*_connection);
        auto result = _work->exec(_query);
        commit();

        return result;
    } catch (const std::exception &e)
    {
        _work.reset();
        throw std::runtime_error(std::string("PostgreSQL query failed: ") + e.what());
    }
}

vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr
vigine::experimental::ecs::postgresql::PostgreSQLComponent::exec(const query::QueryBuilder &queryBuilder)
{
    setQuery(queryBuilder);
    auto result = exec();

    return std::move(result);
}

vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr vigine::experimental::ecs::postgresql::PostgreSQLComponent::connect()
{
    if (!_dbConfig)
        return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                  "database configuration is not initialized");

    auto *connectionData = _dbConfig->connectionData();
    if (!connectionData)
        return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                  "database connection data is not configured");

    try
    {
        _connection = std::make_unique<pqxx::connection>(buildConnectionString(*connectionData));
    } catch (const std::exception &e)
    {
        _connection.reset();
        return std::make_unique<PostgreSQLResult>(
            Result::Code::Error, std::string("PostgreSQL connect failed: ") + e.what());
    }

    initTypeConverter();

    if (!_pgTypeConverter)
        return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                  "postgreSQL type converter should be initialize");

    return std::make_unique<PostgreSQLResult>();
}

vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr vigine::experimental::ecs::postgresql::PostgreSQLComponent::exec()
{
    try
    {
        validateQueryExecution(_connection.get(), _query);

        _work           = std::make_unique<pqxx::work>(*_connection);
        auto pgxxResult = _work->exec(_query);
        commit();

        if (!pgTypeConverter())
            return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                      "PostgreSQL type converter is not initialized");

        return std::make_unique<PostgreSQLResult>(pgxxResult, pgTypeConverter());
    } catch (const std::exception &e)
    {
        _work.reset();
        return std::make_unique<PostgreSQLResult>(
            Result::Code::Error, std::string("PostgreSQL query failed: ") + e.what());
    }
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::commit()
{
    if (!_work)
        return;

    try
    {
        _work->commit();
    } catch (...)
    {
        _work.reset();
        throw;
    }

    _work.reset();
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setQuery(const std::string &query) { _query = query; }

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setHost(const std::string &host) { _host = host; }

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setPort(const std::string &port) { _port = port; }

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setDatabaseName(const std::string &dbName)
{
    _dbName = dbName;
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setDatabaseUser(const std::string &dbUserName)
{
    _dbUserName = dbUserName;
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setPassword(const std::string &password)
{
    _password = password;
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::setPgTypeConverter(
    PostgreSQLTypeConverterUPtr converter)
{
    _pgTypeConverter = std::move(converter);
}

vigine::experimental::ecs::postgresql::PostgreSQLTypeConverterPtr
vigine::experimental::ecs::postgresql::PostgreSQLComponent::pgTypeConverter() const
{
    return _pgTypeConverter.get();
}

void vigine::experimental::ecs::postgresql::PostgreSQLComponent::initTypeConverter() {}

vigine::experimental::ecs::postgresql::DatabaseConfiguration *
vigine::experimental::ecs::postgresql::PostgreSQLComponent::dbConfiguration() const
{
    return _dbConfig.get();
}
