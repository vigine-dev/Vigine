#include "vigine/experimental/ecs/postgresql/impl/data.h"

#include <stdexcept>

vigine::experimental::ecs::postgresql::DataType vigine::experimental::ecs::postgresql::Data::type() const { return _type; }

vigine::experimental::ecs::postgresql::Data::Data(Value val, DataType type) : _value(std::move(val)), _type(type)
{
    bool ok = false;

    if (!_value.valueless_by_exception())
    {
        switch (type)
        {
#define VIGINE_POSTGRESQL_DATA_X(name, type)                                                       \
    case DataType::name:                                                                           \
        ok = std::holds_alternative<type>(_value);                                                 \
        break;

            VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
#undef VIGINE_POSTGRESQL_DATA_X

        default:
            ok = false;
            break;
        }
    }

    if (!ok)
    {
        throw std::invalid_argument(
            "Data constructor: variant type does not match ColumnTypeMap type");
    }
}

vigine::experimental::ecs::postgresql::Data::~Data() {}

bool vigine::experimental::ecs::postgresql::Data::operator==(const Data &other) const
{
    return _value == other._value && _type == other._type;
}

vigine::experimental::ecs::postgresql::TextData::TextData(Value value) : Data(value, DataType::Text) {}

// #define VIGINE_POSTGRESQL_DATA_X(name, type)                                                       \
//     template std::optional<type>                                                                   \
//     vigine::experimental::ecs::postgresql::Data::as<vigine::experimental::ecs::postgresql::DataType::name>() const;

// VIGINE_POSTGRESQL_DATA_COLUMN_TYPE_LIST
// #undef XVIGINE_POSTGRESQL_DATA_X
