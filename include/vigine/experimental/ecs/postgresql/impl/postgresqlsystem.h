#pragma once

/**
 * @file postgresqlsystem.h
 * @brief Declares @c PostgreSQLSystem, the ECS system that wraps the
 *        @c libpqxx client: owns the database configuration and the
 *        per-entity @c PostgreSQLComponent map, and exposes the
 *        connect / schema-check / DDL / query entry points the
 *        database service delegates to.
 */

#include "vigine/ecs/abstractsystem.h"
#include "vigine/experimental/ecs/postgresql/impl/postgresqlresult.h"
#include "vigine/experimental/ecs/postgresql/impl/postgresqltypeconverter.h"

#include <memory>
#include <pqxx/pqxx>
#include <unordered_map>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
class PostgreSQLComponent;
class DatabaseConfiguration;

/**
 * @brief ECS system that talks to a PostgreSQL server through
 *        @c libpqxx and exposes connect / schema / query operations.
 *
 * Hooks into the ECS lifecycle via @ref hasComponents,
 * @ref createComponents, and @ref destroyComponents to track one
 * @c PostgreSQLComponent per bound entity. The public engine API
 * surfaces the database configuration aggregate
 * (@ref dbConfiguration), connection bootstrap (@ref connect), schema
 * validation (@ref checkTablesScheme), and table / query execution
 * helpers. The @c DatabaseService is the usual client and keeps this
 * system off the user-facing surface.
 */
class PostgreSQLSystem : public AbstractSystem
{
  public:
    PostgreSQLSystem(const SystemName &name);
    ~PostgreSQLSystem() override;

    [[nodiscard]] SystemId id() const override;

    // interface implementation
    [[nodiscard]] bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    // custom
    [[nodiscard]] DatabaseConfiguration *dbConfiguration();
    [[nodiscard]] PostgreSQLResultUPtr connect();
    [[nodiscard]] PostgreSQLResultUPtr checkTablesScheme() const;

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

using PostgreSQLSystemUPtr = std::unique_ptr<PostgreSQLSystem>;
using PostgreSQLSystemSPtr = std::shared_ptr<PostgreSQLSystem>;

} // namespace postgresql

} // namespace ecs

} // namespace experimental
}; // namespace vigine
