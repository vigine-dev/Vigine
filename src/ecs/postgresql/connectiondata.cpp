#include "vigine/ecs/postgresql/connectiondata.h"

vigine::postgresql::ConnectionData::ConnectionData() : _dbName(""), _dbUserName(""), _password("")
{
}

void vigine::postgresql::ConnectionData::setHost(const Host &host) { _host = host; }

void vigine::postgresql::ConnectionData::setPort(const Port &port) { _port = port; }

void vigine::postgresql::ConnectionData::setDbName(const Name &name) { _dbName = name; }

void vigine::postgresql::ConnectionData::setDbUserName(const Name &name) { _dbUserName = name; }

void vigine::postgresql::ConnectionData::setPassword(const Password &pass) { _password = pass; }

vigine::Password vigine::postgresql::ConnectionData::password() const { return _password; }

vigine::Name vigine::postgresql::ConnectionData::dbUserName() const { return _dbUserName; }

vigine::Name vigine::postgresql::ConnectionData::dbName() const { return _dbName; }

vigine::postgresql::Port vigine::postgresql::ConnectionData::port() const { return _port; }

vigine::postgresql::Host vigine::postgresql::ConnectionData::host() const { return _host; }
