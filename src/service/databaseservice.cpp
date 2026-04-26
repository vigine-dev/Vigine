#include "vigine/service/databaseservice.h"

#include "vigine/api/context/icontext.h"
#if VIGINE_POSTGRESQL
#include "vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h"
#include <pqxx/pqxx>
#endif

#include <iostream>
#include <string>

#if VIGINE_POSTGRESQL
namespace
{
// Minimum SQL-identifier sanitiser used while the CRUD entry points
// continue to splice table / column names directly into the query
// string (Copilot finding #5 on PR #342). The shipping plan replaces
// the splice with parameterised queries (libpqxx `$1` / `quote_name`)
// in a follow-up; until that lands, every identifier the service
// concatenates passes through this guard so a stray quote / backslash
// / semicolon / `--` / NUL in a caller-supplied @c Name cannot break
// out of its quoted context.
//
// The engine is the only sanctioned producer of these identifiers
// (table names live in @c Table objects the engine creates;
// @c Column names come from @c DatabaseConfiguration the engine wires
// up). The validator therefore acts as a defence-in-depth check, not
// as a substitute for parameterised queries.
//
// Note: PostgreSQL identifiers may contain a wide range of characters
// when properly double-quoted, but the engine convention restricts
// them to ASCII letters, digits, underscore, and hyphen. Allowing the
// full PostgreSQL surface here would require literal "\"" doubling
// (per the libpqxx `quote_name` contract) — out of scope for this
// minimal fix.
[[nodiscard]] bool isPostgresIdentifierSafe(const std::string &identifier)
{
    if (identifier.empty())
        return false;

    for (const char ch : identifier)
    {
        // Reject control characters (NUL included), quotes, backslashes,
        // statement terminators, and SQL line-comment prefixes.
        if (ch == '\0' || ch == '"' || ch == '\'' || ch == '\\' ||
            ch == ';' || ch == '/' || ch == '*')
            return false;

        if (ch < 0x20)
            return false;
    }

    if (identifier.find("--") != std::string::npos)
        return false;

    return true;
}
} // namespace
#endif

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
    std::vector<std::vector<std::string>> resultData;

    // Defence-in-depth identifier guard (Copilot finding #5 on PR #342).
    // The query string is currently spliced rather than parameterised;
    // the validator rejects identifiers that could break out of the
    // double-quoted context. The full parameterised-query refactor is
    // tracked as a follow-up.
    if (!isPostgresIdentifierSafe(tableName))
    {
        std::cerr << "DatabaseService::readData: rejected unsafe table name '"
                  << tableName << "'\n";
        return resultData;
    }

    // Query construction kept inline so the splice is explicit; the
    // current implementation does not yet exercise the bound entity
    // component (the stub returns an empty result set). The full
    // row-fetch wiring is tracked as a follow-up — flagging the
    // unused variable to silence the @c /WX unused-local warning
    // until the fetch path lands.
    std::string query = "SELECT * FROM public.\"" + tableName + "\"";
    static_cast<void>(query);

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

    // Defence-in-depth identifier guard (Copilot finding #5 on PR #342).
    if (!isPostgresIdentifierSafe(tableName))
        return vigine::Result(vigine::Result::Code::Error,
                              "DatabaseService::clearTable: rejected unsafe table name '"
                                  + tableName + "'");

    std::string query = "TRUNCATE TABLE public.\"" + tableName + "\"";

    // Post-#333: queryRequest now returns @c vigine::Result so the
    // unbound-component / driver-error paths surface to the caller
    // (previously the void return silently masked the failure —
    // Copilot finding #2 on PR #342).
    return _postgressSystem->queryRequest(query);
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

    // Hard-coded 3-column shape currently matches the demo schema
    // (Test/id/name/email — see CheckBDShecmeTask). The legacy code
    // called @c columnsData.at(0..2) which throws @c std::out_of_range
    // when fewer columns are supplied, terminating the program with a
    // less actionable error than a Result::Code::Error (Copilot
    // finding #4 on PR #342). Surface the precondition explicitly.
    constexpr std::size_t kRequiredColumns = 3;
    if (columnsData.size() < kRequiredColumns)
        return vigine::Result(vigine::Result::Code::Error,
                              "DatabaseService::writeData: expected at least "
                              + std::to_string(kRequiredColumns) + " columns, got "
                              + std::to_string(columnsData.size()));

    // Defence-in-depth identifier guard (Copilot finding #5 on PR #342).
    if (!isPostgresIdentifierSafe(tableName))
        return vigine::Result(vigine::Result::Code::Error,
                              "DatabaseService::writeData: rejected unsafe table name '"
                                  + tableName + "'");

    // The current splice puts @c Column::name() inside SQL string
    // literals (`'...'`), so the same identifier guard covers the
    // single-quote / backslash / NUL / `--` cases that would let a
    // crafted @c Name break out of the literal. Treating it identically
    // to identifiers is a deliberate over-restriction while the
    // parameterised-query refactor is pending.
    for (std::size_t i = 0; i < kRequiredColumns; ++i)
    {
        const std::string &columnValue = columnsData.at(i).name();
        if (!isPostgresIdentifierSafe(columnValue))
            return vigine::Result(vigine::Result::Code::Error,
                                  "DatabaseService::writeData: rejected unsafe column value at index "
                                      + std::to_string(i) + ": '" + columnValue + "'");
    }

    std::string query = "INSERT INTO public.\"" + tableName + "\"  (col1, col2, col3) VALUES ('" +
                        columnsData.at(0).name() + "', '" + columnsData.at(1).name() + "', '" +
                        columnsData.at(2).name() + "')";

    // Post-#333: queryRequest returns @c vigine::Result; propagate.
    return _postgressSystem->queryRequest(query);
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
