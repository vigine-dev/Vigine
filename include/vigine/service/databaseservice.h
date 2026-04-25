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

#include "vigine/api/service/abstractservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/base/name.h"
#if VIGINE_POSTGRESQL
#include "vigine/experimental/ecs/postgresql/impl/column.h"
#include "vigine/experimental/ecs/postgresql/impl/databaseconfiguration.h"
#include "vigine/experimental/ecs/postgresql/impl/table.h"
#endif
#include "vigine/result.h"

#include <memory>
#include <vector>

namespace vigine
{
#if VIGINE_POSTGRESQL
namespace experimental
{
namespace ecs
{
namespace postgresql
{
class PostgreSQLSystem;
class DatabaseConfiguration;
class Column;
} // namespace postgresql
} // namespace ecs
} // namespace experimental
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
 *
 * Wrapper base (post #330): the service derives from the modern
 * @ref vigine::service::AbstractService (Level-1 wrapper recipe). The
 * legacy @ref vigine::AbstractService base is retired here; callers
 * that previously fetched the service through the pre-R.4.5
 * @ref vigine::Context registry now register it on the new
 * @ref vigine::context::AbstractContext via @c registerService and
 * receive a @ref vigine::service::ServiceId handle. Full modern wiring
 * of the underlying postgres ECS system waits for the architect-
 * approved @c IContext::system() accessor; until that accessor lands,
 * @ref onInit only flips the lifecycle flag and the postgres system
 * pointer is wired up by the caller through a separate path
 * (typically the engine bootstrapper that owns the legacy Context
 * during the transition).
 *
 * Carries an instance @ref vigine::Name supplied at construction so
 * existing call sites that distinguish service instances by name
 * (legacy @c Context::createService used the @c Name pair as the
 * registry key) keep a stable handle.
 */
class DatabaseService : public vigine::service::AbstractService
{
  public:
    explicit DatabaseService(const Name &name);

    /**
     * @brief Returns the instance name supplied at construction.
     *
     * Distinct from the modern @ref vigine::service::IService::id, which
     * is the generational handle stamped by the container during
     * @c registerService. The name preserves the historical
     * "DatabaseService instance called X" handle the legacy registry
     * surfaced.
     */
    [[nodiscard]] const Name &name() const noexcept;

#if VIGINE_POSTGRESQL
    [[nodiscard]] experimental::ecs::postgresql::DatabaseConfiguration *databaseConfiguration();

    [[nodiscard]] ResultUPtr connectToDb();
    [[nodiscard]] ResultUPtr checkDatabaseScheme();
    [[nodiscard]] ResultUPtr createDatabaseScheme();

    void writeData(const std::string &tableName, const std::vector<experimental::ecs::postgresql::Column> columnsData);
    [[nodiscard]] std::vector<std::vector<std::string>>
    readData(const std::string &tableName) const;
    void clearTable(const std::string &tableName) const;

    /**
     * @brief Attaches the @c PostgreSQLSystem this service wraps.
     *
     * Replaces the legacy @c contextChanged path that pulled the
     * postgres system out of @c Context::system. The caller (typically
     * the engine bootstrapper) constructs the system, registers it
     * on the ECS substrate, and hands a non-owning pointer to the
     * service so its CRUD methods can delegate. Passing @c nullptr
     * detaches the system; subsequent CRUD calls report null state
     * the same way they did under the legacy path.
     */
    void setPostgresSystem(experimental::ecs::postgresql::PostgreSQLSystem *system) noexcept;
#endif

    /**
     * @brief Modern lifecycle entry point.
     *
     * Chains to @ref vigine::service::AbstractService::onInit so the
     * @ref isInitialised flag flips to @c true. Concrete domain wiring
     * (postgres-system attachment) is performed through
     * @ref setPostgresSystem; @ref onInit itself does not perform the
     * lookup because @ref vigine::IContext does not yet expose a
     * system locator.
     */
    [[nodiscard]] vigine::Result onInit(vigine::IContext &context) override;

    /**
     * @brief Modern teardown entry point.
     *
     * Drops the postgres-system reference (without owning it) and
     * chains to @ref vigine::service::AbstractService::onShutdown so
     * the @ref isInitialised flag flips back.
     */
    [[nodiscard]] vigine::Result onShutdown(vigine::IContext &context) override;

  private:
    Name _name;
#if VIGINE_POSTGRESQL
    experimental::ecs::postgresql::PostgreSQLSystem *_postgressSystem{nullptr};
#endif
};

using DatabaseServiceUPtr = std::unique_ptr<DatabaseService>;
using DatabaseServiceSPtr = std::shared_ptr<DatabaseService>;

} // namespace vigine
