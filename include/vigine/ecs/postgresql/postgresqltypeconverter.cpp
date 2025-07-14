#include "postgresqltypeconverter.h"

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

vigine::postgresql::PostgreSQLTypeConverter::PostgreSQLTypeConverter() {}

void vigine::postgresql::PostgreSQLTypeConverter::setTypeRelation(BDInternalType internalType,
                                                                  BDExternalType bdExtType)
{
    auto type = pgExternalToVigineDataType(bdExtType);
    if (type == DataType::NotRcognized)
        return;

    _typesContainer.emplace_back(internalType, type);
}

size_t vigine::postgresql::PostgreSQLTypeConverter::size() const { return _typesContainer.size(); }

bool vigine::postgresql::PostgreSQLTypeConverter::empty() const { return _typesContainer.empty(); }

vigine::postgresql::DataType
vigine::postgresql::PostgreSQLTypeConverter::pgExternalToVigineDataType(BDExternalType bdExtType)
{
#define VIGINE_POSTGRESQL_DATA_X(name, type)                                                       \
    if (iequals(bdExtType, #name))                                                                 \
        return DataType::name;

    VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X

    // convert from string to DataType
    return DataType::NotRcognized;
}

std::optional<vigine::postgresql::DataType>
vigine::postgresql::PostgreSQLTypeConverter::toColumnType(BDInternalType internalType) const
{
    for (const auto &[intType, colType] : _typesContainer)
        {
            if (intType == internalType)
                return colType;
        }

    return std::nullopt;
}
