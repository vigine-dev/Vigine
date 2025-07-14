#pragma once

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
