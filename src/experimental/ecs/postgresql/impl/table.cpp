#include "vigine/experimental/ecs/postgresql/impl/table.h"

vigine::Name vigine::experimental::ecs::postgresql::Table::getSchemaName(Schema schema)
{
    if (schema == Schema::Public)
        return Name("public");

    return Name("");
}

vigine::experimental::ecs::postgresql::Table::Table() : _name("") {}

vigine::experimental::ecs::postgresql::Table::Table(const Name &name) : _name{name} {}

bool vigine::experimental::ecs::postgresql::Table::operator==(const Name &name) const { return _name == name; }

bool vigine::experimental::ecs::postgresql::Table::operator!=(const Name &name) const { return !(*this == name); }

bool vigine::experimental::ecs::postgresql::Table::operator!=(const Table &other) const { return !(*this == other); }

bool vigine::experimental::ecs::postgresql::Table::operator==(const Table &other) const
{
    return _name == other._name && _type == other._type && _schema == other._schema &&
           _columns == other._columns;
}

void vigine::experimental::ecs::postgresql::Table::setName(const Name &name) { _name = name; }

const vigine::Name &vigine::experimental::ecs::postgresql::Table::name() const { return _name; }

void vigine::experimental::ecs::postgresql::Table::setType(const Type &type) { _type = type; }

vigine::experimental::ecs::postgresql::Table::Type vigine::experimental::ecs::postgresql::Table::type() const { return _type; }

void vigine::experimental::ecs::postgresql::Table::setSchema(const Schema &schema) { _schema = schema; }

vigine::Name vigine::experimental::ecs::postgresql::Table::schemaName() const { return Table::getSchemaName(_schema); }

void vigine::experimental::ecs::postgresql::Table::addColumn(const Column &column) { _columns.push_back(column); }

vigine::experimental::ecs::postgresql::Table::operator const std::string &() const { return str(); }

vigine::experimental::ecs::postgresql::Table::operator const char *() const { return str().c_str(); }

const std::string &vigine::experimental::ecs::postgresql::Table::str() const { return _name.str(); }

const std::vector<vigine::experimental::ecs::postgresql::Column> &vigine::experimental::ecs::postgresql::Table::columns() const
{
    return _columns;
}

vigine::experimental::ecs::postgresql::Table::Schema vigine::experimental::ecs::postgresql::Table::schema() const { return _schema; }
