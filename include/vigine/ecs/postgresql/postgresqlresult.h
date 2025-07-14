#pragma once

#include "vigine/ecs/postgresql/row.h"
#include "vigine/result.h"

#include <vector>

namespace pqxx
{
class result;
}

namespace vigine
{
namespace postgresql
{
using ColumnName = Name;

class PostgreSQLTypeConverter;

class PostgreSQLResult : public vigine::Result
{
  public:
    using Result::Result;

    PostgreSQLResult();
    PostgreSQLResult(const pqxx::result &data, PostgreSQLTypeConverter *converter);
    ~PostgreSQLResult() override;

    // void setPqxxResult(const pqxx::result &data, PostgreSQLTypeConverter *converter);

    bool empty() const;
    size_t size() const;
    size_t columns() const;
    Row *operator[](int i) const;
    const RowUPtrVector &rows() const;

    Name columnName(size_t index);
    int columnIndex(const Name &name);
    const DataType &columnType(int index) const;
    const DataType &columnType(const Name &name) const;

  private:
    void buildResultData(const pqxx::result &data);

  private:
    PostgreSQLTypeConverter *_converter;
    RowUPtrVector _rows;
};

BUILD_SMART_PTR(PostgreSQLResult);

} // namespace postgresql
} // namespace vigine
