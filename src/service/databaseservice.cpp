#include "vigine/service/databaseservice.h"

#include "vigine/api/context/icontext.h"
#if VIGINE_POSTGRESQL
#include "vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h"
#include <pqxx/pqxx>
#endif

#include <iostream>

vigine::DatabaseService::DatabaseService(const Name &name)
    : vigine::service::AbstractService()
    , _name{name}
{
}

const vigine::Name &vigine::DatabaseService::name() const noexcept { return _name; }

vigine::Result vigine::DatabaseService::onInit(vigine::IContext &context)
{
    // Modern lifecycle: chain to the wrapper base so the
    // @c isInitialised flag flips to @c true. Postgres-system
    // attachment is intentionally NOT performed here; @ref IContext
    // does not yet expose a system locator and the architect-deferred
    // @c IContext::system accessor is out of scope for this leaf.
    // The caller (engine bootstrapper) wires the postgres system in
    // through @ref setPostgresSystem before calling any CRUD method.
    return vigine::service::AbstractService::onInit(context);
}

vigine::Result vigine::DatabaseService::onShutdown(vigine::IContext &context)
{
#if VIGINE_POSTGRESQL
    // Drop the non-owning postgres-system handle before chaining up.
    // The system itself lives on the ECS substrate; we only release
    // our observer pointer so post-shutdown CRUD calls see the same
    // null-state guard the legacy path used to surface.
    _postgressSystem = nullptr;
#endif
    return vigine::service::AbstractService::onShutdown(context);
}

#if VIGINE_POSTGRESQL
void vigine::DatabaseService::setPostgresSystem(
    vigine::experimental::ecs::postgresql::PostgreSQLSystem *system) noexcept
{
    _postgressSystem = system;
}

vigine::ResultUPtr vigine::DatabaseService::checkDatabaseScheme()
{
    if (!_postgressSystem)
        return std::make_unique<Result>(Result::Code::Error,
                                        "DatabaseService: postgres system not attached");

    return _postgressSystem->checkTablesScheme();
}

vigine::ResultUPtr vigine::DatabaseService::createDatabaseScheme()
{
    vigine::ResultUPtr result = std::make_unique<Result>();

    return result;
}

vigine::experimental::ecs::postgresql::DatabaseConfiguration *vigine::DatabaseService::databaseConfiguration()
{
    if (!_postgressSystem)
        return nullptr;

    return _postgressSystem->dbConfiguration();
}

std::vector<std::vector<std::string>>
vigine::DatabaseService::readData(const std::string &tableName) const
{
    std::string query = "SELECT * FROM public.\"" + tableName + "\"";
    std::vector<std::vector<std::string>> resultData;

    return resultData;
}

vigine::Result vigine::DatabaseService::clearTable(const std::string &tableName) const
{
    // Surface the unattached-system case as an explicit error so
    // callers can react instead of treating a no-op as success
    // (Copilot finding A5/B-cluster on PR #331+#332).
    if (!_postgressSystem)
        return vigine::Result(vigine::Result::Code::Error,
                              "DatabaseService::clearTable: postgres system not attached");

    std::string query = "TRUNCATE TABLE public.\"" + tableName + "\"";

    _postgressSystem->queryRequest(query);
    return vigine::Result();
}

vigine::Result vigine::DatabaseService::writeData(
    const std::string &tableName,
    const std::vector<experimental::ecs::postgresql::Column> &columnsData)
{
    // Same surfacing rule as clearTable: explicit error rather than
    // silent void-return when the postgres system is unattached.
    // Parameter changed from by-value to const-ref to avoid copying
    // the columns vector at every call site (Copilot finding A7).
    if (!_postgressSystem)
        return vigine::Result(vigine::Result::Code::Error,
                              "DatabaseService::writeData: postgres system not attached");

    std::string query = "INSERT INTO public.\"" + tableName + "\"  (col1, col2, col3) VALUES ('" +
                        columnsData.at(0).name() + "', '" + columnsData.at(1).name() + "', '" +
                        columnsData.at(2).name() + "')";

    _postgressSystem->queryRequest(query);
    return vigine::Result();
}

vigine::ResultUPtr vigine::DatabaseService::connectToDb()
{
    ResultUPtr result;

    if (!_postgressSystem)
        return std::make_unique<Result>(Result::Code::Error,
                                        "DatabaseService: postgres system not attached");

    try
    {
        result = _postgressSystem->connect();
    } catch (const std::exception &e)
    {
        std::cerr << "DB error: " << e.what() << '\n';
        result = std::make_unique<Result>(Result::Code::Error, e.what());
    }

    return result;
}
#endif
