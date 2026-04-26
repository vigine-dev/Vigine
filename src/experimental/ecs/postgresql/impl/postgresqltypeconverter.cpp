#include "vigine/experimental/ecs/postgresql/impl/postgresqltypeconverter.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>

namespace
{
inline bool iequals(const std::string &a, const std::string &b)
{
    return std::ranges::equal(a, b, [](unsigned char c1, unsigned char c2) {
        return std::tolower(c1) == std::tolower(c2);
    });
}
} // namespace

vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::PostgreSQLTypeConverter() {}

void vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::setTypeRelation(BDInternalType internalType,
                                                                  BDExternalType bdExtType)
{
    auto type = pgExternalToVigineDataType(bdExtType);
    if (type == DataType::NotRecognized)
        return;

    _typesContainer.emplace_back(internalType, type);
}

size_t vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::size() const { return _typesContainer.size(); }

bool vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::empty() const { return _typesContainer.empty(); }

vigine::experimental::ecs::postgresql::DataType
vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::pgExternalToVigineDataType(BDExternalType bdExtType)
{
#define VIGINE_POSTGRESQL_DATA_X(name, type)                                                       \
    if (iequals(bdExtType, #name))                                                                 \
        return DataType::name;

    VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X

    // convert from string to DataType
    return DataType::NotRecognized;
}

std::optional<vigine::experimental::ecs::postgresql::DataType>
vigine::experimental::ecs::postgresql::PostgreSQLTypeConverter::toColumnType(BDInternalType internalType) const
{
    for (const auto &[intType, colType] : _typesContainer)
    {
        if (intType == internalType)
            return colType;
    }

    return std::nullopt;
}
