#pragma once

#include "types/row.hpp"
#include "types/schema.hpp"

#include <string>
#include <vector>

namespace vrdb {

class StorageEngine {
public:
    // Allow derived storage engines to be destroyed through the base interface.
    virtual ~StorageEngine() = default;

    // Create a table CSV file with its schema header.
    virtual void createTable(const std::string& tableName, const Schema& schema) = 0;
    // Validate a row and append its CSV record to the table file.
    virtual void appendRow(const std::string& tableName, const Schema& schema, const Row& row) = 0;
    // Check the table header and deserialize all stored rows.
    virtual std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const = 0;
    // Delete the table storage file or report a failure.
    virtual void dropTable(const std::string& tableName) = 0;
};

} // namespace vrdb
