#include "vigine/experimental/ecs/postgresql/impl/tablerows.h"

vigine::experimental::ecs::postgresql::TableRows::TableRows(const Table &table, const std::vector<Row> &rows)
    : _table(table), _rows(rows)
{
}

bool vigine::experimental::ecs::postgresql::TableRows::operator==(const TableRows &other) const
{
    return _table == other._table && _rows == other._rows;
}

bool vigine::experimental::ecs::postgresql::TableRows::operator!=(const TableRows &other) const
{
    return !(*this == other);
}

void vigine::experimental::ecs::postgresql::TableRows::setTable(const Table &table) { _table = table; }

void vigine::experimental::ecs::postgresql::TableRows::setRows(const std::vector<Row> &rows) { _rows = rows; }

void vigine::experimental::ecs::postgresql::TableRows::addRow(const Row &row) { _rows.push_back(row); }

const vigine::experimental::ecs::postgresql::Row &vigine::experimental::ecs::postgresql::TableRows::operator[](std::size_t index) const
{
    return _rows[index];
}

std::size_t vigine::experimental::ecs::postgresql::TableRows::size() const { return _rows.size(); }

bool vigine::experimental::ecs::postgresql::TableRows::empty() const { return _rows.empty(); }

const vigine::experimental::ecs::postgresql::Row &vigine::experimental::ecs::postgresql::TableRows::at(std::size_t index) const
{
    return _rows.at(index);
}

vigine::experimental::ecs::postgresql::Row &vigine::experimental::ecs::postgresql::TableRows::at(std::size_t index)
{
    return _rows.at(index);
}

vigine::experimental::ecs::postgresql::Row &vigine::experimental::ecs::postgresql::TableRows::operator[](std::size_t index)
{
    return _rows[index];
}

std::vector<vigine::experimental::ecs::postgresql::Row> &vigine::experimental::ecs::postgresql::TableRows::rows() { return _rows; }

const std::vector<vigine::experimental::ecs::postgresql::Row> &vigine::experimental::ecs::postgresql::TableRows::rows() const
{
    return _rows;
}

vigine::experimental::ecs::postgresql::Table &vigine::experimental::ecs::postgresql::TableRows::table() { return _table; }

const vigine::experimental::ecs::postgresql::Table &vigine::experimental::ecs::postgresql::TableRows::table() const { return _table; }
