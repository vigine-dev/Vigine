#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/column.h"
#include "vigine/ecs/postgresql/connectiondata.h"
#include "vigine/ecs/postgresql/table.h"

#include <vector>

namespace vigine
{
namespace postgresql
{
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

BUILD_SMART_PTR(DatabaseConfiguration);
} // namespace postgresql
} // namespace vigine
