#pragma once

#include <string>

namespace vigine
{
namespace postgresql
{
class Column
{
  public:
    enum class Type
    {
        // Number
        Smallint,
        Integer,
        Bigint,
        Numeric,
        Real,
        DoublePrecision,
        // symbol
        Char,
        Varchar,
        Text,
        // boolean
        Boolean,
        // Date and time
        Date,
        Time,
        Timestamp,
        TimestampWithTimeZone,
        Interval,
        // bytes
        Bytea,
        // special types
        Uuid,
        Json,
        Jsonb,
        Array,
        Xml,
        Money,
        Inet,
        Cidr,
        Macaddr,
        // geometric types
        Poin,
        Line,
        Lseg,
        Box,
        Path,
        Polygon,
        Circle,
        // other types

    };

    Column();

    void setName(const std::string &name);
    std::string name() const;

    void setType(const Type &type);
    Type type() const;

    void setPrimary(bool primary);
    bool isPrimary() const;

    void setNullByDefault(bool nullByDefault);
    bool isNullByDefault() const;

    void setUnique(bool unique);
    bool isUnique() const;

    void setForeignKey(bool foreignKey);
    bool isForeignKey() const;

    void setGenerated(bool generated); // IDENTITY
    bool isGenerated() const;

  private:
    std::string _name;
    Type _type{Type::Text};
    bool _primary{false};
    bool _nullByDefault{false};
    bool _unique{false};
    bool _foreignKey{false};
    bool _generated{false};
};
} // namespace postgresql
} // namespace vigine
