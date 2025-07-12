#pragma once

#include <memory>
#include <pqxx/pqxx>
#include <string>

namespace vigine
{
class PostgreSQLComponent
{
  public:
    PostgreSQLComponent();
    ~PostgreSQLComponent();

    void connect();
    pqxx::result exec();
    void commit();

    void setQuery(const std::string &query);
    void setHost(const std::string &host);
    void setPort(const std::string &port);
    void setDatabaseName(const std::string &dbName);
    void setDatabaseUser(const std::string &dbUserName);
    void setPassword(const std::string &password);

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
};
} // namespace vigine
