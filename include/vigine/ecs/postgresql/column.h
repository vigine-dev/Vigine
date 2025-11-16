#pragma once

#include "vigine/base/macros.h"
#include "vigine/base/name.h"
#include "vigine/ecs/postgresql/columntype.h"

#include <string>

namespace vigine
{
namespace postgresql
{
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
} // namespace vigine
