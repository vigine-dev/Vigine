#include "vigine/ecs/postgresql/column.h"

vigine::postgresql::Column::Column(const Name &name) : _name{name} {}

bool vigine::postgresql::Column::operator==(const Column &other) const
{
    return _name == other._name && _type == other._type && _primary == other._primary &&
           _nullByDefault == other._nullByDefault && _unique == other._unique &&
           _foreignKey == other._foreignKey && _generated == other._generated;
}

bool vigine::postgresql::Column::operator!=(const Column &other) const { return !(*this == other); }

void vigine::postgresql::Column::setName(const Name &name) { _name = name; }

const vigine::Name &vigine::postgresql::Column::name() const { return _name; }

void vigine::postgresql::Column::setType(const DataType &type) { _type = type; }

void vigine::postgresql::Column::setPrimary(bool primary) { _primary = primary; }

void vigine::postgresql::Column::setNullByDefault(bool nullByDefault)
{
    _nullByDefault = nullByDefault;
}

bool vigine::postgresql::Column::isNullByDefault() const { return _nullByDefault; }

void vigine::postgresql::Column::setUnique(bool unique) { _unique = unique; }

bool vigine::postgresql::Column::isUnique() const { return _unique; }

void vigine::postgresql::Column::setForeignKey(bool foreignKey) { _foreignKey = foreignKey; }

bool vigine::postgresql::Column::isForeignKey() const { return _foreignKey; }

void vigine::postgresql::Column::setGenerated(bool generated) { _generated = generated; }

bool vigine::postgresql::Column::isGenerated() const { return _generated; }

vigine::postgresql::Column::operator const std::string &() const { return str(); }

vigine::postgresql::Column::operator const char *() const { return str().c_str(); }

const std::string &vigine::postgresql::Column::str() const { return _name.str(); }

vigine::postgresql::DataType vigine::postgresql::Column::type() const { return _type; }
