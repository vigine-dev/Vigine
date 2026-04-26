#pragma once

#include "vigine/api/base/macros.h"
#include "vigine/experimental/ecs/postgresql/impl/databaseconfiguration.h"
#include "vigine/experimental/ecs/postgresql/impl/postgresqlresult.h"
#include "vigine/experimental/ecs/postgresql/impl/postgresqltypeconverter.h"
#include "vigine/experimental/ecs/postgresql/impl/query/querybuilder.h"

#include <memory>
#include <pqxx/pqxx>
#include <string>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{

class DatabaseConfiguration;

class PostgreSQLComponent
{
  public:
    PostgreSQLComponent();
    ~PostgreSQLComponent();

    PostgreSQLResultUPtr connect();
    PostgreSQLResultUPtr exec();
    PostgreSQLResultUPtr exec(const query::QueryBuilder &queryBuilder);
    pqxx::result exec_raw(const query::QueryBuilder &queryBuilder);
    void commit();

    void setQuery(const std::string &query);
    void setHost(const std::string &host);
    void setPort(const std::string &port);
    void setDatabaseName(const std::string &dbName);
    void setDatabaseUser(const std::string &dbUserName);
    void setPassword(const std::string &password);

    void setPgTypeConverter(PostgreSQLTypeConverterUPtr converter);

    DatabaseConfiguration *dbConfiguration() const;
    PostgreSQLTypeConverterPtr pgTypeConverter() const;

  private:
    void initTypeConverter();

  private:
    std::unique_ptr<pqxx::connection> _connection;
    std::unique_ptr<pqxx::work> _work;
    std::unique_ptr<pqxx::result> _result;

    std::string _query;
    std::string _host;
    std::string _port;
    std::string _dbName;
    std::string _dbUserName;
    std::string _password;

    DatabaseConfigurationUPtr _dbConfig;
    PostgreSQLTypeConverterUPtr _pgTypeConverter;
};

using PostgreSQLComponentUPtr = std::unique_ptr<PostgreSQLComponent>;
using PostgreSQLComponentSPtr = std::shared_ptr<PostgreSQLComponent>;

} // namespace postgresql

} // namespace ecs

} // namespace experimental
} // namespace vigine
