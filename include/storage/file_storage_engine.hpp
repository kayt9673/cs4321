#pragma once

#include "storage/csv_format.hpp"

#include <filesystem>
#include <string>

namespace vrdb {

class FileStorageEngine {
public:
    // Set the database directory and create its table and catalog directories if needed.
    explicit FileStorageEngine(std::filesystem::path rootDirectory);

    // Create a table CSV file with its schema header.
    void createTable(const std::string& tableName, const Schema& schema);
    // Validate a row and append its CSV record to the table file.
    RowId appendRow(const std::string& tableName, const Schema& schema, const Row& row);
    bool updateRow(const std::string& tableName, const Schema& schema, RowId id, const Row& row);
    bool deleteRow(const std::string& tableName, const Schema& schema, RowId id);
    // Check the table header and deserialize all stored rows.
    std::vector<StoredRow> readRows(const std::string& tableName, const Schema& schema) const;
    // Delete the table storage file or report a failure.
    void dropTable(const std::string& tableName);

    std::vector<std::string> listTables() const;
    Schema readSchema(const std::string& tableName) const;
    void writeSchema(const std::string& tableName, const Schema& schema);
    void removeSchema(const std::string& tableName);

private:
    CsvFormat::TableData readTable(const std::string& tableName, const Schema& schema) const;
    void writeTable(const std::string& tableName, const Schema& schema,
                    const std::vector<StoredRow>& rows) const;
    RowId readNextId(const std::string& tableName) const;
    void writeNextId(const std::string& tableName, RowId id) const;
    // Build the CSV file path for a table name.
    std::filesystem::path tablePath(const std::string& tableName) const;
    std::filesystem::path nextIdPath(const std::string& tableName) const;
    // Build the catalog file path for a validated table name.
    std::filesystem::path catalogPath(const std::string& tableName) const;

    std::filesystem::path rootDirectory_;
    CsvFormat format_;
};

} // namespace vrdb
