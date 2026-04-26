#pragma once

/**
 * @file connectiondata.h
 * @brief Declares the @c ConnectionData value object that bundles the
 *        PostgreSQL connection parameters (host, port, database name,
 *        user name, password) handed to the @c PostgreSQLSystem.
 */

#include "vigine/api/base/name.h"
#include "vigine/api/base/password.h"

#include <memory>
#include <string>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
using Host = std::string;
using Port = std::string;

/**
 * @brief Holds the parameters required to open a PostgreSQL
 *        connection: host, port, database name, user name, and
 *        password.
 *
 * Plain data container: fields are set via setters and read through
 * matching accessors. The @c Password type (declared under
 * @c vigine/base) wraps the secret so it is not printed accidentally.
 * Owned by the @c DatabaseConfiguration that carries it into the
 * @c PostgreSQLSystem at connect time.
 */
class ConnectionData
{
  public:
    ConnectionData();

    void setHost(const Host &host);
    void setPort(const Port &port);
    void setDbName(const Name &name);
    void setDbUserName(const Name &name);
    void setPassword(const Password &pass);

    Host host() const;
    Port port() const;
    Name dbName() const;
    Name dbUserName() const;
    Password password() const;

  private:
    Host _host;
    Port _port;
    Name _dbName;
    Name _dbUserName;
    Password _password;
};

using ConnectionDataUPtr = std::unique_ptr<ConnectionData>;
using ConnectionDataSPtr = std::shared_ptr<ConnectionData>;
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
