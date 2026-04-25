#pragma once

/**
 * @file tablerows.h
 * @brief Declares @c TableRows, the value object that pairs a
 *        @c Table description with its concrete @c Row set.
 */

#include "vigine/base/macros.h"
#include "vigine/experimental/ecs/postgresql/impl/row.h"
#include "vigine/experimental/ecs/postgresql/impl/table.h"

#include <memory>
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
 * @brief Bundle of a @c Table schema and its ordered @c Row set.
 *
 * Used when a caller needs to carry both the table description and
 * its current rows as a single unit — for example when building an
 * insert payload or when snapshotting a select result together with
 * its schema. The rows are owned by value and can be iterated,
 * indexed, or replaced wholesale via @ref setRows / @ref addRow.
 */
class TableRows
{
  public:
    TableRows() = default;
    TableRows(const Table &table, const std::vector<Row> &rows);

    TableRows(const TableRows &)                = default;
    TableRows &operator=(const TableRows &)     = default;

    TableRows(TableRows &&) noexcept            = default;
    TableRows &operator=(TableRows &&) noexcept = default;

    bool operator==(const TableRows &other) const;
    bool operator!=(const TableRows &other) const;

    void setTable(const Table &table);
    const Table &table() const;
    Table &table();

    const std::vector<Row> &rows() const;
    std::vector<Row> &rows();

    void setRows(const std::vector<Row> &rows);
    void addRow(const Row &row);

    Row &operator[](std::size_t index);
    const Row &operator[](std::size_t index) const;

    Row &at(std::size_t index);
    const Row &at(std::size_t index) const;

    std::size_t size() const;
    bool empty() const;

  private:
    Table _table;
    std::vector<Row> _rows;
};

BUILD_PTR(TableRows);
using TableRowsUPtr = std::unique_ptr<TableRows>;
using TableRowsSPtr = std::shared_ptr<TableRows>;
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
