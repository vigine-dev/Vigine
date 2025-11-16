#include "vigine/ecs/postgresql/row.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <ranges>
#include <format>


namespace vigine::postgresql
{
bool Row::operator==(const Row &other) const { return _columnsData == _columnsData; }

void Row::set(const ColumnName &columnName, const Data &data)
{
    auto index = findIndexByColumnName(columnName);

    if (index == -1)
        {
            _columnsData.emplace_back(columnName, data);

            return;
        }

    _columnsData[index].second = data;
}

const Data &Row::operator[](size_t index) const { return get(index); }

const Data &Row::operator[](const ColumnName &name) const
{
    namespace r = std::ranges;

    auto it = std::ranges::find_if(_columnsData,
                                   [&](auto const& cd) { return cd.first == name; });

    if (it != _columnsData.end()) [[likely]]
        return it->second;

    throw std::out_of_range(std::format("column not found: {}", name.str()));
}

size_t Row::size() const { return _columnsData.size(); }

int Row::columnIndex(const ColumnName &columnName) const { return 0; }

bool Row::empty() const noexcept { return _columnsData.empty(); }

ColumnName Row::columnName(size_t index) const
{
    if (index < _columnsData.size())
        {
            return _columnsData[index].first;
        }

    return "";
}

int Row::findIndexByColumnName(const std::string &name) const
{
    for (size_t i = 0; i < _columnsData.size(); ++i)
        {
            if (_columnsData[i].first == name)
                return i;
        }

    return -1;
}

int Row::findIndexByColumn(const Column &column) const
{
    for (size_t i = 0; i < _columnsData.size(); ++i)
        {
            if (_columnsData[i].first == column)
                return i;
        }

    return -1;
}

Data &Row::get(const Column &column) { return _columnsData[findIndexByColumn(column)].second; }

Data &Row::get(const std::string &columnName)
{
    return _columnsData[findIndexByColumnName(columnName)].second;
}

Data &Row::get(size_t index)
{
    if (index >= _columnsData.size())
        throw std::out_of_range("Index out of bounds");

    return _columnsData[index].second;
}

const Data &Row::get(const std::string &columnName) const
{
    return _columnsData[findIndexByColumnName(columnName)].second;
}

const Data &Row::get(const Column &column) const
{
    return _columnsData[findIndexByColumn(column)].second;
}

const Data &Row::get(size_t index) const
{
    if (index >= _columnsData.size())
        throw std::out_of_range("Index out of bounds");

    return _columnsData[index].second;
}
} // namespace vigine::postgresql
