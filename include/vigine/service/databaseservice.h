#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"
#include "vigine/ecs/entity.h"
#if VIGINE_POSTGRESQL
#include "vigine/ecs/postgresql/column.h"
#include "vigine/ecs/postgresql/databaseconfiguration.h"
#include "vigine/ecs/postgresql/table.h"
#endif
#include "vigine/result.h"

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

BUILD_SMART_PTR(DatabaseService);

} // namespace vigine
