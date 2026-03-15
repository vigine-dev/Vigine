#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"
#include "vigine/ecs/entity.h"
#include "vigine/ecs/postgresql/column.h"
#include "vigine/ecs/postgresql/databaseconfiguration.h"
#include "vigine/ecs/postgresql/table.h"
#include "vigine/result.h"

#include <vector>

namespace vigine
{
namespace postgresql
{
class PostgreSQLSystem;
} // namespace postgresql

class DatabaseService : public AbstractService
{
  public:
    DatabaseService(const Name &name);

    [[nodiscard]] ServiceId id() const override;

    [[nodiscard]] postgresql::DatabaseConfiguration *databaseConfiguration();

    [[nodiscard]] ResultUPtr connectToDb();
    [[nodiscard]] ResultUPtr checkDatabaseScheme();
    [[nodiscard]] ResultUPtr createDatabaseScheme();

    void writeData(const std::string &tableName, const std::vector<postgresql::Column> columnsData);
    [[nodiscard]] std::vector<std::vector<std::string>>
    readData(const std::string &tableName) const;
    void clearTable(const std::string &tableName) const;

  protected:
    void contextChanged() override;
    void entityBound() override;

  private:
    postgresql::PostgreSQLSystem *_postgressSystem{nullptr};
};

BUILD_SMART_PTR(DatabaseService);

} // namespace vigine
