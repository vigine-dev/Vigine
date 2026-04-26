#include "vigine/experimental/ecs/postgresql/impl/column.h"

vigine::experimental::ecs::postgresql::Column::Column(const Name &name) : _name{name} {}

bool vigine::experimental::ecs::postgresql::Column::operator==(const Column &other) const
{
    return _name == other._name && _type == other._type && _primary == other._primary &&
           _nullByDefault == other._nullByDefault && _unique == other._unique &&
           _foreignKey == other._foreignKey && _generated == other._generated;
}

bool vigine::experimental::ecs::postgresql::Column::operator!=(const Column &other) const { return !(*this == other); }

void vigine::experimental::ecs::postgresql::Column::setName(const Name &name) { _name = name; }

const vigine::Name &vigine::experimental::ecs::postgresql::Column::name() const { return _name; }

void vigine::experimental::ecs::postgresql::Column::setType(const DataType &type) { _type = type; }

void vigine::experimental::ecs::postgresql::Column::setPrimary(bool primary) { _primary = primary; }

void vigine::experimental::ecs::postgresql::Column::setNullByDefault(bool nullByDefault)
{
    _nullByDefault = nullByDefault;
}

bool vigine::experimental::ecs::postgresql::Column::isNullByDefault() const { return _nullByDefault; }

void vigine::experimental::ecs::postgresql::Column::setUnique(bool unique) { _unique = unique; }

bool vigine::experimental::ecs::postgresql::Column::isUnique() const { return _unique; }

void vigine::experimental::ecs::postgresql::Column::setForeignKey(bool foreignKey) { _foreignKey = foreignKey; }

bool vigine::experimental::ecs::postgresql::Column::isForeignKey() const { return _foreignKey; }

void vigine::experimental::ecs::postgresql::Column::setGenerated(bool generated) { _generated = generated; }

bool vigine::experimental::ecs::postgresql::Column::isGenerated() const { return _generated; }

vigine::experimental::ecs::postgresql::Column::operator const std::string &() const { return str(); }

vigine::experimental::ecs::postgresql::Column::operator const char *() const { return str().c_str(); }

const std::string &vigine::experimental::ecs::postgresql::Column::str() const { return _name.str(); }

vigine::experimental::ecs::postgresql::DataType vigine::experimental::ecs::postgresql::Column::type() const { return _type; }
