#pragma once

/**
 * @file postgresqlresult.h
 * @brief Declares @c PostgreSQLResult, the engine-side view of a
 *        @c pqxx::result: converts a driver-produced result set into
 *        a sequence of @c Row values keyed by @c DataType-aware
 *        column metadata.
 */

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

/**
 * @brief Query result wrapper: transforms a @c pqxx::result into a
 *        vector of engine-side @c Row objects together with column
 *        metadata (name, index, @c DataType).
 *
 * Constructed by @c PostgreSQLSystem after a successful query. Uses
 * the supplied @c PostgreSQLTypeConverter to translate driver-side
 * type oids into engine @c DataType values at build time so that
 * subsequent row / column accesses are cheap. Inherits from
 * @c vigine::Result so it flows through the engine's uniform result
 * plumbing: a failed query returns a @c Result with an error payload
 * and an empty row set.
 */
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

using PostgreSQLResultUPtr = std::unique_ptr<PostgreSQLResult>;
using PostgreSQLResultSPtr = std::shared_ptr<PostgreSQLResult>;

} // namespace postgresql
} // namespace vigine
