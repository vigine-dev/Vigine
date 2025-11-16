#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"
#include "vigine/ecs/postgresql/postgresqlresult.h"
#include "vigine/ecs/postgresql/postgresqltypeconverter.h"

#include <pqxx/pqxx>
#include <unordered_map>

namespace vigine
{
namespace postgresql
{
class PostgreSQLComponent;
class DatabaseConfiguration;

class PostgreSQLSystem : public AbstractSystem
{
  public:
    PostgreSQLSystem(const SystemName &name);
    ~PostgreSQLSystem() override;

    SystemId id() const override;

    // interface implementation
    bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    // custom
    DatabaseConfiguration *dbConfiguration();
    PostgreSQLResultUPtr connect();
    PostgreSQLResultUPtr checkTablesScheme() const;

    void createTable(const std::string &tableName, const std::vector<std::string> tableColumns);
    void queryRequest(const std::string &query);

  protected:
    virtual void entityBound();
    virtual void entityUnbound();

  private:

    std::vector<std::pair<BDInternalType, BDExternalType>> selectInternalPgTypes();
    PostgreSQLResultUPtr makePgTypeConverter();

  private:
    std::unordered_map<Entity *, std::unique_ptr<PostgreSQLComponent>> _entityComponents;
    PostgreSQLComponent *_boundEntityComponent;
};

BUILD_SMART_PTR(PostgreSQLSystem);

}; // namespace postgresql
}; // namespace vigine
