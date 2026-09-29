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

    // Create storage for a table with the supplied schema.
    virtual void createTable(const std::string& tableName, const Schema& schema) = 0;
    // Validate a row and append it to the table.
    virtual RowId appendRow(const std::string& tableName, const Schema& schema, const Row& row) = 0;
    // Return false when the row ID does not exist.
    virtual bool updateRow(const std::string& tableName, const Schema& schema, RowId id, const Row& row) = 0;
    virtual bool deleteRow(const std::string& tableName, const Schema& schema, RowId id) = 0;
    // Read all stored rows and check them against the schema.
    virtual std::vector<StoredRow> readRows(const std::string& tableName, const Schema& schema) const = 0;
    // Delete the table storage or report a failure.
    virtual void dropTable(const std::string& tableName) = 0;

    // Discover tables from persisted schemas, not from row data alone.
    virtual std::vector<std::string> listTables() const = 0;
    virtual Schema readSchema(const std::string& tableName) const = 0;
    virtual void writeSchema(const std::string& tableName, const Schema& schema) = 0;
    virtual void removeSchema(const std::string& tableName) = 0;
};

} // namespace vrdb
