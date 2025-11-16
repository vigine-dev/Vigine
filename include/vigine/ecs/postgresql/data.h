#pragma once

#include "vigine/base/info.h"
#include "vigine/ecs/postgresql/columntype.h"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace vigine
{
namespace postgresql
{

template <DataType>
struct DataTypeMap
{
    using Type = std::nullptr_t;
};

#define VIGINE_POSTGRESQL_DATA_X(name, type)                                                       \
template <>                                                                                    \
    struct DataTypeMap<DataType::name>                                                             \
{                                                                                              \
        using Type = type;                                                                         \
};

VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X

    template <typename T, typename Variant>
    struct isInVariant;

template <typename T, typename... Ts>
struct isInVariant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...>
{
};

class Data
{
  public:
#define VIGINE_POSTGRESQL_DATA_X(Name, Type) Type,

        using Value = std::variant<VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST Void>;
#undef VIGINE_POSTGRESQL_DATA_X

    explicit Data(Value val, DataType type);
    ~Data();

    DataType type() const;

    template <DataType dataType>
    std::optional<typename DataTypeMap<dataType>::Type> as() const
    {
        if constexpr (info::buildType() == info::BuildType::Debug)
            {
                static_assert(!std::is_same_v<typename DataTypeMap<dataType>::Type, std::nullptr_t>,
                              "Unsupported columnType in ColumnTypeMap");
            }

        if constexpr (std::is_same_v<typename DataTypeMap<dataType>::Type, std::nullptr_t>)
            return std::nullopt;
        else if constexpr (!isInVariant<typename DataTypeMap<dataType>::Type, Value>::value)
            return std::nullopt;
        else
            return std::get<typename DataTypeMap<dataType>::Type>(_value);
    }

    bool operator==(const Data &other) const;

  private:
    Value _value;
    DataType _type;
};

class TextData : public Data
{
  public:
    TextData(Data::Value value);
};
} // namespace postgresql
} // namespace vigine
