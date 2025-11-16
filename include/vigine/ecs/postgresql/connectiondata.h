#pragma once

#include "vigine/base/macros.h"
#include "vigine/base/name.h"
#include "vigine/base/password.h"

#include <string>

namespace vigine
{
namespace postgresql
{
using Host = std::string;
using Port = std::string;

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

BUILD_SMART_PTR(ConnectionData);
} // namespace postgresql
} // namespace vigine
