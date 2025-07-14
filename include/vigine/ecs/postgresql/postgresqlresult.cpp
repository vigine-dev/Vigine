#include "postgresqlresult.h"

#include "vigine/ecs/postgresql/postgresqltypeconverter.h"
#include "vigine/ecs/postgresql/row.h"

#include <iostream>
#include <pqxx/pqxx>
#include <print>
#include <ranges>

vigine::postgresql::PostgreSQLResult::PostgreSQLResult() : vigine::Result() {}

vigine::postgresql::PostgreSQLResult::PostgreSQLResult(const pqxx::result &data,
                                                       PostgreSQLTypeConverter *converter)
    : vigine::Result()
{
    _converter = converter;
    buildResultData(data);
}

vigine::postgresql::PostgreSQLResult::~PostgreSQLResult() {}

// void vigine::postgresql::PostgreSQLResult::setPqxxResult(const pqxx::result &data,
//                                                          PostgreSQLTypeConverter *converter)
// {
//     _converter = converter;
//     buildResultData(data);
// }

bool vigine::postgresql::PostgreSQLResult::empty() const { return _rows.empty(); }

size_t vigine::postgresql::PostgreSQLResult::size() const { return _rows.size(); }

size_t vigine::postgresql::PostgreSQLResult::columns() const
{
    if (empty())
        return 0;

    return _rows.front()->size();
}

vigine::postgresql::Row *vigine::postgresql::PostgreSQLResult::operator [](int i) const
{
    return _rows[i].get();
}

int vigine::postgresql::PostgreSQLResult::columnIndex(const Name &name)
{
    if (empty())
        return -1;

    return _rows.front()->columnIndex(name);
}

vigine::Name vigine::postgresql::PostgreSQLResult::columnName(size_t index)
{
    if (empty())
        return Name("");

    return _rows.front()->columnName(index);
}

const vigine::postgresql::RowUPtrVector &vigine::postgresql::PostgreSQLResult::rows() const
{
    return _rows;
}

void vigine::postgresql::PostgreSQLResult::buildResultData(const pqxx::result &data)
{
    try
        {
            for (auto const &rowData : data)
                {
                    auto row = make_RowUPtr();
                    for (int i = 0; i < data.columns(); ++i)
                        {
                            auto colTypeOID = data.column_type(i);
                            auto columnName = data.column_name(i);

                            switch (_converter->toColumnType(colTypeOID)
                                        .value_or(DataType::NotRcognized))
                                {
                                case DataType::Boolean:
                                    row->set(columnName,
                                             Data(rowData[i].as<bool>(), DataType::Boolean));
                                    break;
                                case DataType::Bigint:
                                    row->set(columnName,
                                             Data(rowData[i].as<int64_t>(), DataType::Bigint));
                                    break;
                                default:
                                    break;
                                }
                        }

                    if (row->size() > 0)
                        _rows.push_back(std::move(row));
                }
        }
    catch (const std::exception &e)
        {
            std::println("Exception: {}", e.what());
        }
}
