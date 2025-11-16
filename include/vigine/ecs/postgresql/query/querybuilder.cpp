#include "vigine/ecs/postgresql/query/querybuilder.h"

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::SELECT(const std::string &what)
{
    _parts.push_back("SELECT " + escapeString(what));

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::FROM(const std::string &table)
{
    const auto result = format("FROM {text}", Data(table, DataType::Text));
    _parts.push_back(result);

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::WHERE(const std::string &condition, const Data &value)
{
    const auto result = format(condition, value);

    _parts.push_back("WHERE " + result);

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::JOIN(const std::string &table)
{
    _parts.push_back("JOIN " + table);
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::ON(const std::string &condition)
{
    _parts.push_back("ON " + condition);
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::GROUP_BY(const std::string &column)
{
    _parts.push_back("GROUP BY " + column);
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::HAVING(const std::string &condition)
{
    _parts.push_back("HAVING " + condition);
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::ORDER_BY(const std::string &column)
{
    _parts.push_back("ORDER BY " + escapeString(column));
    return *this;
}

vigine::postgresql::query::QueryBuilder &vigine::postgresql::query::QueryBuilder::LIMIT(int n)
{
    _parts.push_back("LIMIT " + std::to_string(n));
    return *this;
}

vigine::postgresql::query::QueryBuilder &vigine::postgresql::query::QueryBuilder::OFFSET(int n)
{
    _parts.push_back("OFFSET " + std::to_string(n));
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::INSERT_INTO(const std::string &table,
                                                     const std::map<std::string, Data> &values)
{
    std::ostringstream columns, vals;
    columns << "(";
    vals << "(";
    bool first = true;
    for (const auto &[col, val] : values)
        {
            if (!first)
                {
                    columns << ", ";
                    vals << ", ";
                }
            columns << col;
            vals << escape(val);
            first = false;
        }
    columns << ")";
    vals << ")";
    _parts.push_back("INSERT INTO " + table + " " + columns.str() + " VALUES " + vals.str());
    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::SET(const std::string &column, Operation op,
                                             const Data &value)
{
    std::string opStr = operationToString(op);
    _parts.push_back("SET " + column + " " + opStr + " " + escape(value));

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::SELECT_EXISTS(const QueryBuilder &subquery)
{
    _parts.push_back("SELECT EXISTS ( " + subquery + " )");

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::AND(const std::string &condition, const Data &value)
{
    const auto result = format(condition, value);

    _parts.push_back("AND " + result);

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::AS(const std::string &name)
{
    _parts.push_back(" AS " + escapeString(name));

    return *this;
}

vigine::postgresql::query::QueryBuilder &vigine::postgresql::query::QueryBuilder::COMMA()
{
    _parts.push_back(", ");

    return *this;
}

vigine::postgresql::query::QueryBuilder &
vigine::postgresql::query::QueryBuilder::NAME(const std::string &name)
{
    _parts.push_back(" " + escapeString(name) + " ");

    return *this;
}

std::string vigine::postgresql::query::QueryBuilder::str() const
{
    std::ostringstream result;
    for (const auto &part : _parts)
        {
            result << part << " ";
        }
    std::string query = result.str();
    if (!query.empty())
        query.pop_back();

    return query;
}

vigine::postgresql::query::QueryBuilder::operator std::string() const
{
    try
        {
            return str();
        }
    catch (...)
        {
            return "cast to string error";
        }
}

void vigine::postgresql::query::QueryBuilder::reset() { _parts.clear(); }

bool vigine::postgresql::query::QueryBuilder::isQueryValid() const { return !str().empty(); }

std::string vigine::postgresql::query::QueryBuilder::operationToString(Operation op) const
{
    switch (op)
        {
        case Operation::equal:
            return "=";
        case Operation::not_equal:
            return "!=";
        case Operation::less:
            return "<";
        case Operation::greater:
            return ">";
        case Operation::less_equal:
            return "<=";
        case Operation::greater_equal:
            return ">=";
        }
    return "=";
}

std::string vigine::postgresql::query::QueryBuilder::format(std::string_view templateStr,
                                                            const Data &param) const
{
    using namespace std::string_view_literals;

    // clang-format off
    std::unordered_map<std::string_view, std::function<std::string(const Data &)>>
        tagHandlers {
            {"table_name"sv, [this](const Data &data) -> std::string  { return std::format("'{}'", escapeString(data.as<DataType::Text>().value_or(""))); }},
            {"quoted"sv,     [this](const Data &data) -> std::string  { return std::format("'{}'", escape(data)); } },
            {"text"sv,       [](const Data &data)     -> std::string  { return data.as<DataType::Text>().value_or(""); }},
            {"is_null"sv,    [](const Data &data)     -> std::string  { return "TRUE"; }                                },
            {"bool"sv,       [](const Data &data)     -> std::string  { return (data.as<DataType::Boolean>().value_or(false))?"true":"false"; }     },
            {"char"sv,       [](const Data &data)     -> std::string  { return "'" + std::string(1, data.as<DataType::Char>().value_or('\0')) + "'"; }},
    };
    // clang-format on

    std::string result;
    size_t pos = 0;

    while (pos < templateStr.size())
        {
            size_t braceOpen = templateStr.find('{', pos);
            if (braceOpen == std::string_view::npos)
                {
                    result.append(templateStr.substr(pos));
                    break;
                }

            result.append(templateStr.substr(pos, braceOpen - pos));
            size_t braceClose = templateStr.find('}', braceOpen);
            if (braceClose == std::string_view::npos)
                {
                    throw std::invalid_argument("Tag is missing closing brace");

                    break;
                }

            std::string_view tag = templateStr.substr(braceOpen + 1, braceClose - braceOpen - 1);
            if (auto it = tagHandlers.find(tag); it != tagHandlers.end())
                {
                    result.append(it->second(param));
                }
            else
                {
                    throw std::invalid_argument("Tag is unknown");

                    // result.append(templateStr.substr(braceOpen, braceClose - braceOpen + 1));
                }

            pos = braceClose + 1;
        }

    return result;
}

std::string vigine::postgresql::query::QueryBuilder::escape(const Data &data) const
{
    if (auto str = data.as<DataType::Text>())
        {
            return "'" + escapeString(*str) + "'";
        }
    else if (auto i = data.as<DataType::Integer>())
        {
            return std::to_string(*i);
        }
    return "NULL";
}

std::string vigine::postgresql::query::QueryBuilder::escapeString(const std::string &input) const
{
    std::string output;
    for (char c : input)
        {
            if (c == '\'' || c == '\\')
                output += '\\';
            output += c;
        }
    return output;
}

std::string vigine::postgresql::query::operator+(const std::string &lhs, const QueryBuilder &rhs)
{
    return lhs + rhs.str();
}

std::string vigine::postgresql::query::operator+(const QueryBuilder &lhs, const std::string &rhs)
{
    return lhs.str() + rhs;
}

std::string vigine::postgresql::query::operator+(const char *lhs, const QueryBuilder &rhs)
{
    return std::string(lhs) + rhs.str();
}

std::string vigine::postgresql::query::operator+(const QueryBuilder &lhs, const char *rhs)
{
    return lhs.str() + std::string(rhs);
}
