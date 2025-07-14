#include "vigine/ecs/postgresql/table.h"


std::string vigine::postgresql::Table::getSchemaName(Schema schema)
{
    if (schema == Schema::Public)
        return "public";

    return "";
}

vigine::postgresql::Table::Table() {}

void vigine::postgresql::Table::setName(const std::string &name) { _name = name; }

std::string vigine::postgresql::Table::name() const { return _name; }

void vigine::postgresql::Table::setType(const Type &type) { _type = type; }

vigine::postgresql::Table::Type vigine::postgresql::Table::type() const { return _type; }

void vigine::postgresql::Table::setSchema(const Schema &schema) { _schema = schema; }

void vigine::postgresql::Table::addColumn(const Column &column) { _columns.push_back(column); }

vigine::postgresql::Table::Schema vigine::postgresql::Table::schema() const { return _schema; }
