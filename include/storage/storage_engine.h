#pragma once

#include "types/row.h"
#include "types/schema.h"

#include <string>
#include <vector>

namespace vrdb {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual void createTable(const std::string& tableName, const Schema& schema) = 0;
    virtual RowId appendRow(const std::string& tableName, const Schema& schema, const Row& row) = 0;
    virtual bool updateRow(const std::string& tableName, const Schema& schema, RowId id, const Row& row) = 0;
    virtual bool deleteRow(const std::string& tableName, const Schema& schema, RowId id) = 0;
    virtual std::vector<StoredRow> readRows(const std::string& tableName, const Schema& schema) const = 0;
    virtual void dropTable(const std::string& tableName) = 0;
};

} // namespace vrdb
