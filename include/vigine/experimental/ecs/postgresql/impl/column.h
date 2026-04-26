#pragma once

/**
 * @file column.h
 * @brief Declares the @c Column value type describing a single
 *        PostgreSQL table column (name, data type, and per-column
 *        flags such as primary, unique, foreign key, generated,
 *        default-null).
 */

#include "vigine/api/base/macros.h"
#include "vigine/api/base/name.h"
#include "vigine/experimental/ecs/postgresql/impl/columntype.h"

#include <string>

namespace vigine
{
namespace experimental
{
namespace ecs
{
namespace postgresql
{
/**
 * @brief Value object describing one column of a PostgreSQL table.
 *
 * Stores the column name, its @ref DataType, and the per-column
 * flags the engine needs to emit DDL and to validate read / write
 * paths: @c primary, @c unique, @c foreignKey, @c generated
 * (identity), and @c nullByDefault. Columns are plain values: copy,
 * move, and the default relational operators behave as expected.
 * The string conversion operators and @ref str return the column
 * name so a @c Column can stand in for a column identifier in query
 * string builders.
 */
class Column
{
  public:
    Column()                              = default;
    Column(const Column &)                = default;
    Column(Column &&) noexcept            = default;
    Column &operator=(const Column &)     = default;
    Column &operator=(Column &&) noexcept = default;

    Column(const Name &name);

    bool operator==(const Column &other) const;
    bool operator!=(const Column &other) const;

    void setName(const Name &name);
    const Name &name() const;

    void setType(const DataType &type);
    DataType type() const;

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

    operator const std::string &() const;
    operator const char *() const;
    const std::string &str() const;

  private:
    Name _name;
    DataType _type{DataType::Text};
    bool _primary{false};
    bool _nullByDefault{true};
    bool _unique{false};
    bool _foreignKey{false};
    bool _generated{false};
};

BUILD_PTR(Column);
} // namespace postgresql
} // namespace ecs
} // namespace experimental
} // namespace vigine
