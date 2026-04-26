#pragma once

/**
 * @file databaseservice.h
 * @brief Concrete service that exposes database operations (connect,
 *        scheme check / create, row read / write / clear) through the
 *        service container.
 *
 * Two-tier gating chain:
 *   1. The experimental postgres example (@c example/experimental/postgres_demo)
 *      is opted in at the top-level CMake through
 *      @c VIGINE_ENABLE_EXPERIMENTAL combined with
 *      @c BUILD_EXAMPLE_POSTGRESQL (see @c example/CMakeLists.txt).
 *   2. Inside this header, the database-facing CRUD API is compiled in
 *      only when the @c VIGINE_POSTGRESQL preprocessor define is set.
 *      When the experimental umbrella is enabled the build system
 *      defines @c VIGINE_POSTGRESQL automatically; otherwise this
 *      service still exists as a lifecycle-only stub but its
 *      database-facing API is omitted.
 *
 * The two flags are NOT redundant: @c VIGINE_ENABLE_EXPERIMENTAL gates
 * the experimental tree as a whole (across multiple subsystems), while
 * @c VIGINE_POSTGRESQL is the per-translation-unit compile-time switch
 * that this header keys off.
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
 * Wrapper base: the service derives from
 * @ref vigine::service::AbstractService (Level-1 wrapper recipe).
 * Callers register it on the @ref vigine::context::AbstractContext
 * via @c registerService and receive a
 * @ref vigine::service::ServiceId handle. Full wiring of the
 * underlying postgres ECS system waits for the architect-approved
 * @c IContext::system() accessor; until that accessor lands,
 * @ref onInit only flips the lifecycle flag and the postgres system
 * pointer is wired up by the caller through a separate path
 * (typically the engine bootstrapper).
 *
 * Carries an instance @ref vigine::Name supplied at construction so
 * call sites that distinguish service instances by name keep a stable
 * handle.
 */
class DatabaseService : public vigine::service::AbstractService
{
  public:
    explicit DatabaseService(const Name &name);

    /**
     * @brief Returns the instance name supplied at construction.
     *
     * Distinct from @ref vigine::service::IService::id, which is the
     * generational handle stamped by the container during
     * @c registerService.
     */
    [[nodiscard]] const Name &name() const noexcept;

#if VIGINE_POSTGRESQL
    [[nodiscard]] experimental::ecs::postgresql::DatabaseConfiguration *databaseConfiguration();

    [[nodiscard]] ResultUPtr connectToDb();
    [[nodiscard]] ResultUPtr checkDatabaseScheme();
    [[nodiscard]] ResultUPtr createDatabaseScheme();

    /**
     * @brief Inserts a row into the named table.
     *
     * Returns @c Result::Code::Error when the postgres system is
     * unattached (previously the call returned @c void and silently
     * dropped the request — a real CRUD failure was indistinguishable
     * from a successful no-op). The @c columnsData parameter is taken
     * by const-reference to avoid a vector copy on every call site.
     */
    [[nodiscard]] vigine::Result writeData(const std::string &tableName,
                                           const std::vector<experimental::ecs::postgresql::Column> &columnsData);
    [[nodiscard]] std::vector<std::vector<std::string>>
    readData(const std::string &tableName) const;

    /**
     * @brief Truncates the named table.
     *
     * Returns @c Result::Code::Error when the postgres system is
     * unattached (previously @c void with a silent return, hiding the
     * failure). Callers can chain on @ref vigine::Result::isError to
     * surface the issue up the task graph.
     */
    [[nodiscard]] vigine::Result clearTable(const std::string &tableName) const;

    /**
     * @brief Attaches the @c PostgreSQLSystem this service wraps.
     *
     * The caller (typically the engine bootstrapper) constructs the
     * system, registers it on the ECS substrate, and hands a
     * non-owning pointer to the service so its CRUD methods can
     * delegate. Passing @c nullptr detaches the system; subsequent
     * CRUD calls report null state.
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
