#include "vigine/ecs/postgresql/column.h"

vigine::postgresql::Column::Column() {}

void vigine::postgresql::Column::setName(const std::string &name) { _name = name; }

std::string vigine::postgresql::Column::name() const { return _name; }

void vigine::postgresql::Column::setType(const Type &type) { _type = type; }

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

vigine::postgresql::Column::Type vigine::postgresql::Column::type() const { return _type; }
