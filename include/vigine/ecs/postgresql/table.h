#pragma once

#include "vigine/base/name.h"
#include "vigine/ecs/postgresql/column.h"

#include <string>
#include <vector>

namespace vigine
{
namespace postgresql
{
class Table
{
  public:
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
} // namespace vigine
