#pragma once

#include <cstdint>

#define VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST                                                    \
    VIGINE_POSTGRESQL_DATA_X(Integer, int)                                                         \
    VIGINE_POSTGRESQL_DATA_X(Bigint, int64_t)                                                     \
    VIGINE_POSTGRESQL_DATA_X(Text, std::string)                                                    \
    VIGINE_POSTGRESQL_DATA_X(Boolean, bool)                                                        \
    VIGINE_POSTGRESQL_DATA_X(Char, char)

namespace vigine
{
namespace postgresql
{
struct Void
{
    constexpr bool operator==(const Void &) const noexcept { return true; }
};

enum class DataType
{
    NotRcognized,
#define VIGINE_POSTGRESQL_DATA_X(name, type) name,

    VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X
};
} // namespace postgresql
} // namespace vigine
