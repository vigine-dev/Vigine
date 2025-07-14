#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/postgresql/columntype.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vigine
{
namespace postgresql
{
using BDInternalType = int;
using BDExternalType = std::string;

class PostgreSQLTypeConverter
{
  public:
    PostgreSQLTypeConverter();

    void setTypeRelation(BDInternalType internalType, BDExternalType bdExtType);
    std::optional<DataType> toColumnType(BDInternalType internalType) const;
    DataType pgExternalToVigineDataType(BDExternalType bdExtType);
    size_t size() const;
    bool empty() const;

  private:
    std::vector<std::pair<BDInternalType, DataType>> _typesContainer;
};

BUILD_PTR(PostgreSQLTypeConverter);
BUILD_SMART_PTR(PostgreSQLTypeConverter);
} // namespace postgresql
} // namespace vigine
