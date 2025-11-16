#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/data.h"

#include <deque>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vigine
{
namespace postgresql
{
namespace query
{
class QueryBuilder;

enum class Operation
{
    equal,
    not_equal,
    less,
    greater,
    less_equal,
    greater_equal
};

class QueryBuilder
{
  public:
    QueryBuilder() = default;

    QueryBuilder &SELECT(const std::string &what);
    QueryBuilder &SELECT_EXISTS(const QueryBuilder &subquery);
    QueryBuilder &FROM(const std::string &table);
    QueryBuilder &WHERE(const std::string &condition, const Data &value);
    QueryBuilder &AND(const std::string &condition, const Data &value);
    QueryBuilder &AS(const std::string &name);
    QueryBuilder &JOIN(const std::string &table);
    QueryBuilder &ON(const std::string &condition);
    QueryBuilder &GROUP_BY(const std::string &column);
    QueryBuilder &ORDER_BY(const std::string &column);
    QueryBuilder &HAVING(const std::string &condition);
    QueryBuilder &LIMIT(int n);
    QueryBuilder &OFFSET(int n);
    QueryBuilder &INSERT_INTO(const std::string &table, const std::map<std::string, Data> &values);
    QueryBuilder &SET(const std::string &column, Operation op, const Data &value);
    QueryBuilder &COMMA();
    QueryBuilder &NAME(const std::string &name);

    std::string str() const;
    operator std::string() const;

    void reset();
    bool isQueryValid() const;

  private:
    std::string escape(const Data &data) const;
    std::string escapeString(const std::string &input) const;
    std::string operationToString(Operation op) const;

    std::string format(std::string_view templateStr, const Data &param) const;

  private:
    std::deque<std::string> _parts;
};

BUILD_SMART_PTR(QueryBuilder);

std::string operator+(const std::string &lhs, const QueryBuilder &rhs);
std::string operator+(const QueryBuilder &lhs, const std::string &rhs);
std::string operator+(const char *lhs, const QueryBuilder &rhs);
std::string operator+(const QueryBuilder &lhs, const char *rhs);
} // namespace query
} // namespace postgresql
} // namespace vigine
