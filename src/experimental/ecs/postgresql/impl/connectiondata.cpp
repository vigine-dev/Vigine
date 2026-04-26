#include "vigine/experimental/ecs/postgresql/impl/connectiondata.h"

vigine::experimental::ecs::postgresql::ConnectionData::ConnectionData() : _dbName(""), _dbUserName(""), _password("")
{
}

void vigine::experimental::ecs::postgresql::ConnectionData::setHost(const Host &host) { _host = host; }

void vigine::experimental::ecs::postgresql::ConnectionData::setPort(const Port &port) { _port = port; }

void vigine::experimental::ecs::postgresql::ConnectionData::setDbName(const Name &name) { _dbName = name; }

void vigine::experimental::ecs::postgresql::ConnectionData::setDbUserName(const Name &name) { _dbUserName = name; }

void vigine::experimental::ecs::postgresql::ConnectionData::setPassword(const Password &pass) { _password = pass; }

vigine::Password vigine::experimental::ecs::postgresql::ConnectionData::password() const { return _password; }

vigine::Name vigine::experimental::ecs::postgresql::ConnectionData::dbUserName() const { return _dbUserName; }

vigine::Name vigine::experimental::ecs::postgresql::ConnectionData::dbName() const { return _dbName; }

vigine::experimental::ecs::postgresql::Port vigine::experimental::ecs::postgresql::ConnectionData::port() const { return _port; }

vigine::experimental::ecs::postgresql::Host vigine::experimental::ecs::postgresql::ConnectionData::host() const { return _host; }
