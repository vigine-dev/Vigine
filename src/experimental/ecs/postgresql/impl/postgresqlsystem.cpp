#include "vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h"

#include "vigine/experimental/ecs/postgresql/impl/data.h"
#include "vigine/experimental/ecs/postgresql/impl/postgresqltypeconverter.h"
#include "vigine/experimental/ecs/postgresql/impl/query/querybuilder.h"
#include "vigine/experimental/ecs/postgresql/impl/row.h"

#include "experimental/ecs/postgresql/impl/postgresqlcomponent.h"

#include <iostream>
#include <print>
#include <vector>

// COPILOT_TODO: Ініціалізувати _boundEntityComponent в nullptr у initializer list, як це вже
// зроблено в WindowSystem, щоб не покладатися на невизначений стан.
vigine::experimental::ecs::postgresql::PostgreSQLSystem::PostgreSQLSystem(const SystemName &name)
    : AbstractSystem(name), _boundEntityComponent(nullptr)
{
}

vigine::experimental::ecs::postgresql::PostgreSQLSystem::~PostgreSQLSystem() {}

bool vigine::experimental::ecs::postgresql::PostgreSQLSystem::hasComponents(Entity *entity) const
{
    if (!entity || _entityComponents.empty())
        return false;

    return _entityComponents.contains(entity);
}

void vigine::experimental::ecs::postgresql::PostgreSQLSystem::createComponents(Entity *entity)
{
    if (!entity)
        return;

    // TODO: check correct work of this method

    auto pgComponent     = std::make_unique<PostgreSQLComponent>();
    auto pgTypeConverter = std::make_unique<PostgreSQLTypeConverter>();

    pgComponent->setPgTypeConverter(std::move(pgTypeConverter));

    _entityComponents[entity] = std::move(pgComponent);
}

void vigine::experimental::ecs::postgresql::PostgreSQLSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    _entityComponents.erase(entity);
}

// COPILOT_TODO: Прибрати суперечливу перевірку res.size() > 0 / res.empty() і додати guard на
// _boundEntityComponent перед setPgTypeConverter().
vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr vigine::experimental::ecs::postgresql::PostgreSQLSystem::makePgTypeConverter()
{
    if (auto res = selectInternalPgTypes(); res.size() > 0)
    {
        if (res.empty())
            return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                      "Didn't select internal postgres types");

        auto typeConverter = std::make_unique<PostgreSQLTypeConverter>();
        if (!typeConverter)
            return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                      "Didn't create postgres type converter");

        for (const auto &item : res)
        {
            typeConverter->setTypeRelation(item.first, item.second);
            println("this is repchick: {}:{}", item.first, item.second);
        }

        if (typeConverter->empty())
            return std::make_unique<PostgreSQLResult>(
                Result::Code::Error, "Type converter is empty. You can't continue working.");

        _boundEntityComponent->setPgTypeConverter(std::move(typeConverter));
    }

    return std::make_unique<PostgreSQLResult>();
}

vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr
vigine::experimental::ecs::postgresql::PostgreSQLSystem::checkTablesScheme() const
{
    // Defensive null-check: previously this routine dereferenced
    // _boundEntityComponent directly and crashed when the system had
    // no entity bound yet (Copilot finding A9). Surface the error as
    // a Result so the call chain through DatabaseService::checkDatabaseScheme
    // returns cleanly.
    if (!_boundEntityComponent)
        return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                  "PostgreSQL entity component is not bound");

    const auto &tables = _boundEntityComponent->dbConfiguration()->tables();

    bool hasError{false};
    std::string errorMessage;

    for (const auto &table : tables)
    {
        query::QueryBuilder mainQueryBuilder;
        query::QueryBuilder subQueryBuilder;

        {
            subQueryBuilder.SELECT("1")
                .FROM("pg_catalog.pg_tables")
                .WHERE("schemaname = {table_name}", TextData(table.schemaName()))
                .AND("tablename = {table_name}", TextData(table.name()));

            mainQueryBuilder.SELECT_EXISTS(subQueryBuilder);
        }

        auto result = _boundEntityComponent->exec(mainQueryBuilder);

        if (result->empty())
        {
            hasError      = true;
            errorMessage += "table: " + table.name() + " - has wrong schema or is absent. ";
            continue;
        }

        auto row = (*result)[0];
        if (row && !row->empty())
        {
            auto data     = row->operator[]("exists");
            auto dataType = data.type();

            if (dataType != DataType::Boolean)
            {
                hasError      = true;
                errorMessage += "table: " + table.name() + " - has wrong schema or is absent. ";
                continue;
            }

            auto value = data.as<DataType::Boolean>();
            if (!value)
            {
                hasError      = true;
                errorMessage += "table: " + table.name() + " - has wrong schema or is absent. ";
                continue;
            }

            // check the result
            {
                // build cols name sql query
            }
        }

        std::vector<std::string> cols =
            table.columns() | std::views::transform([](const auto &col) { return col.name(); }) |
            std::ranges::to<std::vector<std::string>>();

        std::string colsStr;
        for (int i = 0; i < cols.size(); ++i)
        {
            colsStr += "'" + cols[i] + "'";
            if ((i + 1) < cols.size())
                colsStr += ",";
        }

        {
            subQueryBuilder.reset();
            subQueryBuilder.SELECT("COUNT(*)")
                .FROM("information_schema.columns")
                .WHERE("table_schema = {table_name}", TextData(table.schemaName()))
                .AND("table_name = {table_name}", TextData(table.name()))
                .AND("column_name IN ({text})", TextData(colsStr));
        }

        result = _boundEntityComponent->exec(subQueryBuilder);

        if (result->empty())
        {
            hasError = true;
            errorMessage +=
                "table: " + table.name() + " - invalid columns detected. Please verify them. ";
            continue;
        }
    }

    const auto code = (hasError) ? vigine::Result::Code::Error : vigine::Result::Code::Success;

    return std::make_unique<PostgreSQLResult>(code, errorMessage);
}

// COPILOT_TODO: Захистити цей accessor від nullptr або змінити контракт так, щоб без bindEntity()
// його було неможливо викликати.
vigine::experimental::ecs::postgresql::DatabaseConfiguration *vigine::experimental::ecs::postgresql::PostgreSQLSystem::dbConfiguration()
{
    if (!_boundEntityComponent)
        return nullptr;

    return _boundEntityComponent->dbConfiguration();
}

// COPILOT_TODO: Якщо система ще не прив'язана до entity, тут потрібно повертати помилку явно, а не
// дереференсити _boundEntityComponent всліпу.
vigine::experimental::ecs::postgresql::PostgreSQLResultUPtr vigine::experimental::ecs::postgresql::PostgreSQLSystem::connect()
{
    if (!_boundEntityComponent)
        return std::make_unique<PostgreSQLResult>(Result::Code::Error,
                                                  "PostgreSQL entity component is not bound");

    auto result = _boundEntityComponent->connect();

    if (result->isSuccess())
    {
        if (auto res = makePgTypeConverter(); res->isError())
            return std::move(res);
    }

    return std::move(result);
}

// COPILOT_TODO: Додати перевірку _boundEntityComponent перед setQuery/exec/commit, інакше
// createTable аварійно падає при неправильному порядку викликів.
void vigine::experimental::ecs::postgresql::PostgreSQLSystem::createTable(const std::string &tableName,
                                                       const std::vector<std::string> tableColumns)
{
    if (!_boundEntityComponent)
    {
        std::println("PostgreSQL createTable skipped: entity component is not bound");
        return;
    }

    std::string cols;
    for (size_t i = 0; i < tableColumns.size(); ++i)
    {
        cols += "" + tableColumns[i] + " TEXT";
        if (i + 1 < tableColumns.size())
            cols += ",";
    }

    std::string query = "CREATE TABLE IF NOT EXISTS public.\"" + tableName + "\" (" + cols + ")";

    _boundEntityComponent->setQuery(query);
    auto result = _boundEntityComponent->exec();

    if (result->isError())
        std::println("PostgreSQL createTable failed: {}", result->message());
}

// Post-#333: signature returns @c vigine::Result so callers can react
// to the unbound-state and driver-error paths instead of treating a
// silent no-op as success.
vigine::Result
vigine::experimental::ecs::postgresql::PostgreSQLSystem::queryRequest(const std::string &query)
{
    if (!_boundEntityComponent)
    {
        std::println("PostgreSQL query skipped: entity component is not bound");
        return vigine::Result(vigine::Result::Code::Error,
                              "PostgreSQLSystem::queryRequest: no entity component is bound");
    }

    _boundEntityComponent->setQuery(query);
    auto result = _boundEntityComponent->exec();

    if (!result)
        return vigine::Result(vigine::Result::Code::Error,
                              "PostgreSQLSystem::queryRequest: driver returned a null result");

    if (result->isError())
    {
        std::println("PostgreSQL query failed: {}", result->message());
        return vigine::Result(vigine::Result::Code::Error, result->message());
    }

    return vigine::Result();
}

void vigine::experimental::ecs::postgresql::PostgreSQLSystem::entityBound()
{
    auto boundEntity      = getBoundEntity();
    _boundEntityComponent = nullptr;

    if (_entityComponents.contains(boundEntity))
        _boundEntityComponent = _entityComponents.at(boundEntity).get();
}

void vigine::experimental::ecs::postgresql::PostgreSQLSystem::entityUnbound() { _boundEntityComponent = nullptr; }

std::vector<std::pair<vigine::experimental::ecs::postgresql::BDInternalType, vigine::experimental::ecs::postgresql::BDExternalType>>
vigine::experimental::ecs::postgresql::PostgreSQLSystem::selectInternalPgTypes()
{
    std::vector<std::pair<vigine::experimental::ecs::postgresql::BDInternalType, vigine::experimental::ecs::postgresql::BDExternalType>>
        types;

    query::QueryBuilder mainQueryBuilder;
    mainQueryBuilder.SELECT("t.oid")
        .AS("internal_type")
        .COMMA()
        .NAME("format_type(t.oid, NULL)")
        .AS("pg_display_type")
        .FROM("pg_type t")
        .WHERE("t.typisdefined = {bool}", Data(true, DataType::Boolean))
        .AND("t.typtype = {char}", Data('b', DataType::Char))
        .ORDER_BY("t.typname ASC");

    auto result = _boundEntityComponent->exec_raw(mainQueryBuilder);

    if (!result.empty())
    {
        try
        {
            int rows = result.size();
            int y    = rows;

            for (int row = 0; row < rows; ++row, --y)
            {
                if (result.empty() || result[row][0].is_null())
                    continue;

                auto internal = result[row][0].as<int>();
                auto external = result[row][1].as<std::string>();

                types.emplace_back(internal, external);
            }
        } catch (const std::exception &e)
        {
            std::println("Exception: {}", e.what());
        }
    } else
    {

        std::println("result successfull {}", "false");
    }

    return types;
}

vigine::SystemId vigine::experimental::ecs::postgresql::PostgreSQLSystem::id() const { return "PostgreSQL"; }
