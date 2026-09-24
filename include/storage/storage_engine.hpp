#pragma once

#include "types/row.hpp"
#include "types/schema.hpp"

#include <string>
#include <vector>

namespace vrdb {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual void createTable(const std::string& tableName, const Schema& schema) = 0;
    virtual void appendRow(const std::string& tableName, const Schema& schema, const Row& row) = 0;
    virtual std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const = 0;
    virtual void dropTable(const std::string& tableName) = 0;
};

} // namespace vrdb
