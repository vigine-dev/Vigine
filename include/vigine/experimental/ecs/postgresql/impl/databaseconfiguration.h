#pragma once

/**
 * @file databaseconfiguration.h
 * @brief Declares the @c DatabaseConfiguration aggregate that couples
 *        a list of expected @c Table definitions with the
 *        @c ConnectionData needed to reach the PostgreSQL server.
 */

#include "vigine/base/macros.h"
#include "vigine/experimental/ecs/postgresql/impl/column.h"
#include "vigine/experimental/ecs/postgresql/impl/connectiondata.h"
#include "vigine/experimental/ecs/postgresql/impl/table.h"

#include <vector>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
/**
 * @brief Aggregate describing the database a @c PostgreSQLSystem
 *        connects to.
 *
 * Carries two pieces: the owned @c ConnectionData (how to reach the
 * server) and the list of @c Table definitions the engine expects to
 * find (used by scheme-check and DDL-create paths). Ownership of the
 * connection data is held through a @c std::unique_ptr; the table
 * list is owned by value. The service that uses this configuration
 * (see @c DatabaseService) is responsible for populating it before
 * calling connect.
 */
class DatabaseConfiguration
{
  public:
    DatabaseConfiguration();
    ~DatabaseConfiguration();
    void setTables(const std::vector<Table> &tables);
    void setConnectionData(ConnectionDataUPtr &&connectionData);
    ConnectionData *connectionData() const;
    const std::vector<Table> &tables() const;

  private:
    std::vector<Table> _tables;
    ConnectionDataUPtr _connectionData;
};

using DatabaseConfigurationUPtr = std::unique_ptr<DatabaseConfiguration>;
using DatabaseConfigurationSPtr = std::shared_ptr<DatabaseConfiguration>;
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
