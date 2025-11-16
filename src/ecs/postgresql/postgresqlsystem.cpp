#include "vigine/ecs/postgresql/postgresqlsystem.h"

#include "vigine/ecs/postgresql/data.h"
#include "vigine/ecs/postgresql/postgresqltypeconverter.h"
#include "vigine/ecs/postgresql/query/querybuilder.h"
#include "vigine/ecs/postgresql/row.h"

#include "ecs/postgresql/postgresqlcomponent.h"

#include <iostream>
#include <print>
#include <vector>

vigine::postgresql::PostgreSQLSystem::PostgreSQLSystem(const SystemName &name)
    : AbstractSystem(name)
{
}

vigine::postgresql::PostgreSQLSystem::~PostgreSQLSystem() {}

bool vigine::postgresql::PostgreSQLSystem::hasComponents(Entity *entity) const
{
    if (!entity || _entityComponents.empty())
        return false;

    return _entityComponents.contains(entity);
}

void vigine::postgresql::PostgreSQLSystem::createComponents(Entity *entity)
{
    if (!entity)
        return;

    // TODO: check correct work of this method

    auto pgComponent     = make_PostgreSQLComponentUPtr();
    auto pgTypeConverter = make_PostgreSQLTypeConverterUPtr();

    pgComponent->setPgTypeConverter(std::move(pgTypeConverter));

    _entityComponents[entity] = std::move(pgComponent);
}

void vigine::postgresql::PostgreSQLSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    _entityComponents.erase(entity);
}

vigine::postgresql::PostgreSQLResultUPtr vigine::postgresql::PostgreSQLSystem::makePgTypeConverter()
{
    if (auto res = selectInternalPgTypes(); res.size() > 0)
        {
            if (res.empty())
                return make_PostgreSQLResultUPtr(Result::Code::Error,
                                                 "Didn't select internal postgres types");

            auto typeConverter = make_PostgreSQLTypeConverterUPtr();
            if (!typeConverter)
                return make_PostgreSQLResultUPtr(Result::Code::Error,
                                                 "Didn't create postgres type converter");

            for (const auto &item : res)
                {
                    typeConverter->setTypeRelation(item.first, item.second);
                    println("this is repchick: {}:{}", item.first, item.second);
                }

            if (typeConverter->empty())
                return make_PostgreSQLResultUPtr(
                    Result::Code::Error, "Type converter is empty. You can't continue working.");

            _boundEntityComponent->setPgTypeConverter(std::move(typeConverter));
        }

    return make_PostgreSQLResultUPtr();
}

vigine::postgresql::PostgreSQLResultUPtr
vigine::postgresql::PostgreSQLSystem::checkTablesScheme() const
{
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
                            hasError = true;
                            errorMessage +=
                                "table: " + table.name() + " - has wrong schema or is absent. ";
                            continue;
                        }

                    auto value = data.as<DataType::Boolean>();
                    if (!value)
                        {
                            hasError = true;
                            errorMessage +=
                                "table: " + table.name() + " - has wrong schema or is absent. ";
                            continue;
                        }

                    // check the result
                    {
                        // build cols name sql query
                    }
                }

            std::vector<std::string> cols =
                table.columns() |
                std::views::transform([](const auto &col) { return col.name(); }) |
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
                    hasError      = true;
                    errorMessage += "table: " + table.name() +
                                    " - invalid columns detected. Please verify them. ";
                    continue;
                }
        }

    const auto code = (hasError) ? vigine::Result::Code::Error : vigine::Result::Code::Success;

    return make_PostgreSQLResultUPtr(code, errorMessage);
}

vigine::postgresql::DatabaseConfiguration *vigine::postgresql::PostgreSQLSystem::dbConfiguration()
{
    return _boundEntityComponent->dbConfiguration();
}

vigine::postgresql::PostgreSQLResultUPtr vigine::postgresql::PostgreSQLSystem::connect()
{
    auto result = _boundEntityComponent->connect();

    if (result->isSuccess())
        {
            if (auto res = makePgTypeConverter(); res->isError())
                return std::move(res);
        }

    return std::move(result);
}

void vigine::postgresql::PostgreSQLSystem::createTable(const std::string &tableName,
                                                       const std::vector<std::string> tableColumns)
{
    std::string cols;
    for (size_t i = 0; i < tableColumns.size(); ++i)
        {
            cols += "" + tableColumns[i] + " TEXT";
            if (i + 1 < tableColumns.size())
                cols += ",";
        }

    std::string query = "CREATE TABLE IF NOT EXISTS public.\"" + tableName + "\" (" + cols + ")";

    _boundEntityComponent->setQuery(query);
    _boundEntityComponent->exec();
    _boundEntityComponent->commit();
}

void vigine::postgresql::PostgreSQLSystem::queryRequest(const std::string &query)
{
    _boundEntityComponent->setQuery(query);
    _boundEntityComponent->exec();
    _boundEntityComponent->commit();
}

void vigine::postgresql::PostgreSQLSystem::entityBound()
{
    auto boundEntity      = getBoundEntity();
    _boundEntityComponent = nullptr;

    if (_entityComponents.contains(boundEntity))
        _boundEntityComponent = _entityComponents.at(boundEntity).get();
}

void vigine::postgresql::PostgreSQLSystem::entityUnbound() { _boundEntityComponent = nullptr; }

std::vector<std::pair<vigine::postgresql::BDInternalType, vigine::postgresql::BDExternalType>>
vigine::postgresql::PostgreSQLSystem::selectInternalPgTypes()
{
    std::vector<std::pair<vigine::postgresql::BDInternalType, vigine::postgresql::BDExternalType>>
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
                }
            catch (const std::exception &e)
                {
                    std::println("Exception: {}", e.what());
                }
        }
    else
        {

            std::println("result successfull {}", "false");
        }

    return types;
}

vigine::SystemId vigine::postgresql::PostgreSQLSystem::id() const { return "PostgreSQL"; }
