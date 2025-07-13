#pragma once

#include "vigine/result.h"
#include <vigine/abstractservice.h>
#include <vigine/ecs/entity.h>

#include <vector>

namespace vigine
{

class PostgreSQLSystem;

using Column = std::string;

struct Table
{
    std::string name;
    std::vector<Column> columns;
};

class DatabaseService : public AbstractService
{
  public:
    DatabaseService(const ServiceName &name);

    void contextChanged() override;

    ServiceId id() const override;
    Result connectToDb();
    bool checkTablesExist(const std::vector<Table> &tables) const;
    void createTables(const std::vector<Table> &tables) const;
    void insertData(const std::string &tableName, const std::vector<vigine::Column> columnsData);

  protected:
    void entityBound() override;

  private:
    PostgreSQLSystem *_postgressSystem{nullptr};
};
} // namespace vigine
