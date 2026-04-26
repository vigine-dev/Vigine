#pragma once

/**
 * @file querybuilder.h
 * @brief Declares the @c QueryBuilder DSL for assembling PostgreSQL
 *        statements (SELECT, WHERE / AND, JOIN / ON, GROUP BY,
 *        ORDER BY, HAVING, LIMIT / OFFSET, INSERT INTO, SET) from a
 *        chain of fluent calls that takes @c Data values directly.
 */

#include "vigine/api/base/macros.h"
#include "vigine/experimental/ecs/postgresql/impl/data.h"

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
namespace experimental
{
namespace ecs
{
namespace postgresql
{
namespace query
{
class QueryBuilder;

/**
 * @brief Comparison operator a caller passes to @ref QueryBuilder::SET
 *        when expressing "column @c op value" fragments.
 */
enum class Operation
{
    equal,
    not_equal,
    less,
    greater,
    less_equal,
    greater_equal
};

/**
 * @brief Fluent builder that assembles a PostgreSQL statement out of
 *        chained clause-shaped calls (@ref SELECT, @ref FROM,
 *        @ref WHERE, @ref AND, @ref JOIN, @ref ORDER_BY,
 *        @ref INSERT_INTO, and so on) and renders the final text
 *        through @ref str.
 *
 * Clause methods append to an internal deque of fragments; @ref str
 * joins the fragments. Value-bearing clauses accept a @ref Data and
 * route it through @ref escape so callers do not build SQL strings
 * by hand. @ref isQueryValid exposes a lightweight structural check
 * intended for call-site sanity asserts, not full SQL validation.
 */
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

    [[nodiscard]] std::string str() const;
    [[nodiscard]] operator std::string() const;

    void reset();
    [[nodiscard]] bool isQueryValid() const;

  private:
    std::string escape(const Data &data) const;
    std::string escapeString(const std::string &input) const;
    std::string operationToString(Operation op) const;

    std::string format(std::string_view templateStr, const Data &param) const;

  private:
    std::deque<std::string> _parts;
};

using QueryBuilderUPtr = std::unique_ptr<QueryBuilder>;
using QueryBuilderSPtr = std::shared_ptr<QueryBuilder>;

std::string operator+(const std::string &lhs, const QueryBuilder &rhs);
std::string operator+(const QueryBuilder &lhs, const std::string &rhs);
std::string operator+(const char *lhs, const QueryBuilder &rhs);
std::string operator+(const QueryBuilder &lhs, const char *rhs);
} // namespace query
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
