#pragma once

/**
 * @file postgresqltypeconverter.h
 * @brief Declares @c PostgreSQLTypeConverter, the lookup object that
 *        maps PostgreSQL internal type oids (and their textual names)
 *        to engine-side @c DataType values.
 */

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

/**
 * @brief Bidirectional lookup between PostgreSQL type oids (and their
 *        external textual names) and the engine's @ref DataType.
 *
 * Seeded at connect time by @c PostgreSQLSystem (it queries
 * @c pg_type once and records the server-local oids because oids are
 * not stable across installations). @ref toColumnType resolves an
 * internal oid to a @c DataType, returning @c std::nullopt when the
 * oid has no matching engine type. @ref pgExternalToVigineDataType
 * performs the same mapping from the textual side and also records
 * the resolved pair so subsequent oid lookups succeed.
 */
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
using PostgreSQLTypeConverterUPtr = std::unique_ptr<PostgreSQLTypeConverter>;
using PostgreSQLTypeConverterSPtr = std::shared_ptr<PostgreSQLTypeConverter>;
} // namespace postgresql
} // namespace vigine
