#pragma once

/**
 * @file table.h
 * @brief Declares @c Table, the value object describing a PostgreSQL
 *        table (name, storage type, schema, and ordered column list)
 *        used when the engine builds DDL or validates a live schema.
 */

#include "vigine/base/name.h"
#include "vigine/experimental/ecs/postgresql/impl/column.h"

#include <string>
#include <vector>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
/**
 * @brief Value object describing one PostgreSQL table: name, storage
 *        type, schema (namespace), and ordered list of @c Column
 *        definitions.
 *
 * Constructed by callers that declare a database layout; consumed by
 * @c DatabaseConfiguration and then by @c PostgreSQLSystem when it
 * emits DDL or validates the live schema matches expectation. The
 * string conversion operators and @ref str return the table name so
 * a @c Table can stand in for a table identifier in query string
 * builders.
 */
class Table
{
  public:
    /**
     * @brief PostgreSQL table storage type.
     *
     * Mirrors the PostgreSQL concepts the engine has to emit when
     * creating a table; @ref Regular is the plain persistent table
     * used by default.
     */
    enum class Type
    {
        Regular,      // by default
        Unlogged,     // without logging
        Temporary,    // is only in the session
        Partitioned,  // divide by parts
        Inherited,    // inherit structure of the other table
        Foreign,      // for external data
        Materialized, // saved query result
        View          // virtual
    };

    /**
     * @brief PostgreSQL schema (namespace) that owns the table.
     *
     * Only the built-in @c public schema is supported today;
     * @ref Custom is reserved for future user-defined schemas and
     * currently routes through the same code path as @ref Public.
     */
    enum class Schema
    {
        Public, // by default
        Custom  // can add a custom scheme. Implement if needed
    };

    static Name getSchemaName(Schema schema);

    Table();
    Table(const Name &name);

    Table(const Table &)                = default;
    Table &operator=(const Table &)     = default;
    Table(Table &&) noexcept            = default;
    Table &operator=(Table &&) noexcept = default;

    bool operator==(const Table &other) const;
    bool operator!=(const Table &other) const;
    bool operator==(const Name &name) const;
    bool operator!=(const Name &name) const;

    void setName(const Name &name);
    const Name &name() const;

    void setType(const Type &type);
    Type type() const;

    void setSchema(const Schema &schema);
    Schema schema() const;
    Name schemaName() const;

    void addColumn(const Column &column);
    const std::vector<Column> &columns() const;

    operator const std::string &() const;
    operator const char *() const;
    const std::string &str() const;

  private:
    Name _name;
    Type _type{Type::Regular};
    Schema _schema{Schema::Public};
    std::vector<Column> _columns;
};
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
