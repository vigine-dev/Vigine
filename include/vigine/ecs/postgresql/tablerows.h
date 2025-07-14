#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/row.h"
#include "vigine/ecs/postgresql/table.h"

#include <vector>
#include <memory>

namespace vigine
{
namespace postgresql
{
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
BUILD_SMART_PTR(TableRows);
} // namespace postgresql
} // namespace vigine
