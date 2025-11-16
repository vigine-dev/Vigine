#include "tablerows.h"

vigine::postgresql::TableRows::TableRows(const Table &table, const std::vector<Row> &rows)
    : _table(table), _rows(rows)
{
}

bool vigine::postgresql::TableRows::operator==(const TableRows &other) const
{
    return _table == other._table && _rows == other._rows;
}

bool vigine::postgresql::TableRows::operator!=(const TableRows &other) const
{
    return !(*this == other);
}

void vigine::postgresql::TableRows::setTable(const Table &table) { _table = table; }

void vigine::postgresql::TableRows::setRows(const std::vector<Row> &rows) { _rows = rows; }

void vigine::postgresql::TableRows::addRow(const Row &row) { _rows.push_back(row); }

const vigine::postgresql::Row &vigine::postgresql::TableRows::operator[](std::size_t index) const
{
    return _rows[index];
}

std::size_t vigine::postgresql::TableRows::size() const { return _rows.size(); }

bool vigine::postgresql::TableRows::empty() const { return _rows.empty(); }

const vigine::postgresql::Row &vigine::postgresql::TableRows::at(std::size_t index) const
{
    return _rows.at(index);
}

vigine::postgresql::Row &vigine::postgresql::TableRows::at(std::size_t index)
{
    return _rows.at(index);
}

vigine::postgresql::Row &vigine::postgresql::TableRows::operator[](std::size_t index)
{
    return _rows[index];
}

std::vector<vigine::postgresql::Row> &vigine::postgresql::TableRows::rows() { return _rows; }

const std::vector<vigine::postgresql::Row> &vigine::postgresql::TableRows::rows() const
{
    return _rows;
}

vigine::postgresql::Table &vigine::postgresql::TableRows::table() { return _table; }

const vigine::postgresql::Table &vigine::postgresql::TableRows::table() const { return _table; }
