#include "vigine/ecs/postgresql/table.h"

vigine::Name vigine::postgresql::Table::getSchemaName(Schema schema)
{
    if (schema == Schema::Public)
        return Name("public");

    return Name("");
}

vigine::postgresql::Table::Table() : _name("") {}

vigine::postgresql::Table::Table(const Name &name) : _name{name} {}

bool vigine::postgresql::Table::operator==(const Name &name) const { return _name == name; }

bool vigine::postgresql::Table::operator!=(const Name &name) const { return !(*this == name); }

bool vigine::postgresql::Table::operator!=(const Table &other) const { return !(*this == other); }

bool vigine::postgresql::Table::operator==(const Table &other) const
{
    return _name == other._name && _type == other._type && _schema == other._schema &&
           _columns == other._columns;
}

void vigine::postgresql::Table::setName(const Name &name) { _name = name; }

const vigine::Name &vigine::postgresql::Table::name() const { return _name; }

void vigine::postgresql::Table::setType(const Type &type) { _type = type; }

vigine::postgresql::Table::Type vigine::postgresql::Table::type() const { return _type; }

void vigine::postgresql::Table::setSchema(const Schema &schema) { _schema = schema; }

vigine::Name vigine::postgresql::Table::schemaName() const { return Table::getSchemaName(_schema); }

void vigine::postgresql::Table::addColumn(const Column &column) { _columns.push_back(column); }

vigine::postgresql::Table::operator const std::string &() const { return str(); }

vigine::postgresql::Table::operator const char *() const { return str().c_str(); }

const std::string &vigine::postgresql::Table::str() const { return _name.str(); }

const std::vector<vigine::postgresql::Column> &vigine::postgresql::Table::columns() const
{
    return _columns;
}

vigine::postgresql::Table::Schema vigine::postgresql::Table::schema() const { return _schema; }
