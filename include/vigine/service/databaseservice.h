#pragma once

/**
 * @file databaseservice.h
 * @brief Concrete service that exposes database operations (connect,
 *        scheme check / create, row read / write / clear) through the
 *        service container.
 *
 * Database operations are compiled in only when the project is built
 * with @c VIGINE_POSTGRESQL enabled; otherwise the service still exists
 * but its database-facing API is omitted.
 */

#include "vigine/abstractservice.h"
#include "vigine/ecs/entity.h"
#if VIGINE_POSTGRESQL
#include "vigine/ecs/postgresql/column.h"
#include "vigine/ecs/postgresql/databaseconfiguration.h"
#include "vigine/ecs/postgresql/table.h"
#endif
#include "vigine/result.h"

#include <memory>
#include <vector>

namespace vigine
{
#if VIGINE_POSTGRESQL
namespace postgresql
{
class PostgreSQLSystem;
class DatabaseConfiguration;
class Column;
} // namespace postgresql
#endif

/**
 * @brief Database service: wraps a @c PostgreSQLSystem and exposes
 *        database connect / schema / row CRUD operations to callers
 *        through the service container.
 *
 * Holds no database state of its own; delegates every operation to
 * the owned @c postgresql::PostgreSQLSystem. The database-facing API
 * is only compiled when @c VIGINE_POSTGRESQL is enabled at build time.
 * When the build flag is off the service still registers with the
 * container but exposes only the lifecycle surface inherited from
 * @c AbstractService.
 */
class DatabaseService : public AbstractService
{
  public:
    DatabaseService(const Name &name);

    [[nodiscard]] ServiceId id() const override;

#if VIGINE_POSTGRESQL
    [[nodiscard]] postgresql::DatabaseConfiguration *databaseConfiguration();

    [[nodiscard]] ResultUPtr connectToDb();
    [[nodiscard]] ResultUPtr checkDatabaseScheme();
    [[nodiscard]] ResultUPtr createDatabaseScheme();

    void writeData(const std::string &tableName, const std::vector<postgresql::Column> columnsData);
    [[nodiscard]] std::vector<std::vector<std::string>>
    readData(const std::string &tableName) const;
    void clearTable(const std::string &tableName) const;
#endif

  protected:
    void contextChanged() override;
    void entityBound() override;

  private:
#if VIGINE_POSTGRESQL
    postgresql::PostgreSQLSystem *_postgressSystem{nullptr};
#endif
};

using DatabaseServiceUPtr = std::unique_ptr<DatabaseService>;
using DatabaseServiceSPtr = std::shared_ptr<DatabaseService>;

} // namespace vigine
