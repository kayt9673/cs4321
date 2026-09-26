#pragma once

#include "storage/storage_engine.hpp"

#include <filesystem>
#include <string>

namespace vrdb {

class FileStorageEngine : public StorageEngine {
public:
    // Set the table storage directory and create it if needed.
    explicit FileStorageEngine(std::filesystem::path rootDirectory);

    // Create a table CSV file with its schema header.
    void createTable(const std::string& tableName, const Schema& schema) override;
    // Validate a row and append its CSV record to the table file.
    void appendRow(const std::string& tableName, const Schema& schema, const Row& row) override;
    // Check the table header and deserialize all stored rows.
    std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const override;
    // Delete the table storage file or report a failure.
    void dropTable(const std::string& tableName) override;

private:
    // Build the CSV file path for a table name.
    std::filesystem::path tablePath(const std::string& tableName) const;

    std::filesystem::path rootDirectory_;
};

} // namespace vrdb
