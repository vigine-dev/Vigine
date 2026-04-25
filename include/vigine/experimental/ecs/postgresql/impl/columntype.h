#pragma once

/**
 * @file columntype.h
 * @brief X-macro list and @c DataType enum declaring the PostgreSQL
 *        column types the engine recognises.
 *
 * The @c VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST X-macro enumerates
 * each supported column kind together with its C++ type; it is
 * expanded below to generate the @c DataType enum and is reused
 * elsewhere (see @c data.h) to drive the @c std::variant data
 * payload and the @c DataTypeMap trait table.
 */

#include <cstdint>

#define VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST                                                    \
    VIGINE_POSTGRESQL_DATA_X(Integer, int)                                                         \
    VIGINE_POSTGRESQL_DATA_X(Bigint, int64_t)                                                      \
    VIGINE_POSTGRESQL_DATA_X(Text, std::string)                                                    \
    VIGINE_POSTGRESQL_DATA_X(Boolean, bool)                                                        \
    VIGINE_POSTGRESQL_DATA_X(Char, char)

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
/**
 * @brief Empty placeholder appended to the column-type @c std::variant
 *        so the variant always has at least one default-constructible
 *        alternative (needed when the X-macro list is otherwise empty
 *        or when a value is not yet bound to a type).
 */
struct Void
{
    constexpr bool operator==(const Void &) const noexcept { return true; }
};

/**
 * @brief Closed enum of PostgreSQL column types supported by the
 *        engine.
 *
 * @c NotRcognized is the sentinel returned when the engine cannot
 * map a database-side type to one of the recognised kinds; the
 * remaining enumerators are generated from
 * @c VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST and mirror the C++
 * types that the @c Data payload stores.
 */
enum class DataType
{
    NotRcognized,
#define VIGINE_POSTGRESQL_DATA_X(name, type) name,

    VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X
};
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
