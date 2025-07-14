#pragma once

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

    static std::string getSchemaName(Schema schema);

    Table();

    void setName(const std::string &name);
    std::string name() const;

    void setType(const Type &type);
    Type type() const;

    void setSchema(const Schema &schema);
    Schema schema() const;

    void addColumn(const Column &column);

  private:
    std::string _name;
    Type _type{Type::Regular};
    Schema _schema{Schema::Public};
    std::vector<Column> _columns;
};
} // namespace postgresql
} // namespace vigine
