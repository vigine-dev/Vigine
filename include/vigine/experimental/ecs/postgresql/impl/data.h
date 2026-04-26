#pragma once

/**
 * @file data.h
 * @brief Declares @c Data (a type-tagged PostgreSQL cell value) along
 *        with the @c DataTypeMap trait that maps a @ref DataType enum
 *        to its concrete C++ representation.
 */

#include "vigine/api/base/info.h"
#include "vigine/experimental/ecs/postgresql/impl/columntype.h"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{

/**
 * @brief Trait mapping a @ref DataType enumerator to the C++ type
 *        that represents values of that kind.
 *
 * The primary template resolves to @c std::nullptr_t, acting as the
 * "unmapped" sentinel. Specialisations for every entry of
 * @c VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST are generated just
 * below via an X-macro, so adding a new column type in one place
 * automatically extends the trait.
 */
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

/**
 * @brief Compile-time predicate: @c true when @p T appears in the
 *        alternatives of the given @c std::variant, @c false
 *        otherwise.
 *
 * Used by @ref Data::as to short-circuit to @c std::nullopt when a
 * caller asks for a type that the @c Data variant cannot hold.
 */
template <typename T, typename Variant>
struct isInVariant;

template <typename T, typename... Ts>
struct isInVariant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...>
{
};

/**
 * @brief Type-tagged cell value: stores one PostgreSQL datum together
 *        with its @ref DataType, and exposes a typed accessor
 *        @ref as<dataType> that returns @c std::optional of the
 *        mapped C++ type.
 *
 * The payload is a @c std::variant covering every type listed by
 * @c VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST plus the empty @c Void
 * alternative. The tag (@ref type) records which @ref DataType the
 * caller attached to the value. @ref as performs compile-time
 * validation through @ref DataTypeMap and @ref isInVariant and
 * returns @c std::nullopt for unmapped / unsupported kinds.
 */
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

/**
 * @brief Convenience subclass of @ref Data that forces the tag to
 *        @c DataType::Text.
 *
 * Useful at call sites that build a @ref Data from a
 * @c std::string-valued @c Data::Value and want to avoid repeating
 * the @c DataType::Text tag.
 */
class TextData : public Data
{
  public:
    TextData(Data::Value value);
};
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
