#pragma once

/**
 * @file row.h
 * @brief Declares @c Row, the engine-side representation of a single
 *        result or insert row: an ordered sequence of
 *        (column-name, @c Data) pairs with index- and name-based
 *        access.
 */

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/column.h"
#include "vigine/ecs/postgresql/data.h"

#include <memory>
#include <vector>

namespace vigine
{
namespace postgresql
{
using ColumnName = vigine::Name;
using ColumnData = std::pair<ColumnName, Data>;

/**
 * @brief One database row as an ordered list of
 *        (@ref ColumnName, @ref Data) pairs, addressable by integer
 *        index, by @c Column, or by column name.
 *
 * Rows are constructed by @c PostgreSQLResult when translating a
 * @c pqxx::result into engine types, and by callers that assemble
 * rows for insert paths via @ref set. Access helpers (@ref get,
 * @ref operator[]) return the matching @c Data; name-based lookup is
 * linear in the row width. The row has no notion of a schema; each
 * entry carries its own column name so the row is self-describing.
 */
class Row
{
  public:
    Row()                           = default;
    Row(const Row &)                = default;
    Row(Row &&) noexcept            = default;
    Row &operator=(const Row &)     = default;
    Row &operator=(Row &&) noexcept = default;
    ~Row()                          = default;

    bool operator==(const Row &other) const;

    void set(const ColumnName &columnName, const Data &data);

    const Data &get(size_t index) const;
    const Data &get(const Column &column) const;
    const Data &get(const std::string &columnName) const;
    Data &get(size_t index);
    Data &get(const Column &column);
    Data &get(const std::string &columnName);

    const Data &operator[](size_t index) const;
    Data &operator[](size_t index);

    const Data &operator[](const ColumnName &columnName) const;

    size_t size() const;
    int columnIndex(const ColumnName &columnName) const;
    ColumnName columnName(size_t index) const;
    bool empty() const noexcept;

  private:
    int findIndexByColumnName(const std::string &name) const;
    int findIndexByColumn(const Column &column) const;

  private:
    std::vector<ColumnData> _columnsData;
};

BUILD_SMART_PTR(Row);
BUILD_PTR_VECTOR_VECTOR(Row);
} // namespace postgresql
} // namespace vigine
