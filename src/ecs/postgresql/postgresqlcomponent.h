#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/databaseconfiguration.h"
#include "vigine/ecs/postgresql/postgresqlresult.h"
#include "vigine/ecs/postgresql/postgresqltypeconverter.h"
#include "vigine/ecs/postgresql/query/querybuilder.h"

#include <memory>
#include <pqxx/pqxx>
#include <string>

namespace vigine
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

BUILD_SMART_PTR(PostgreSQLComponent);

} // namespace postgresql
} // namespace vigine
