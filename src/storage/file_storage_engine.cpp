#include "storage/file_storage_engine.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <fstream>
#include <utility>

namespace vrdb {

// Set the table storage directory and create it if needed.
FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_{std::move(rootDirectory)} {
    std::filesystem::create_directories(rootDirectory_);
}

// Create a table CSV file with its schema header.
void FileStorageEngine::createTable(const std::string& tableName, const Schema& schema) {
    std::filesystem::create_directories(rootDirectory_);

    std::ofstream file{tablePath(tableName), std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to create table storage for " + tableName};
    }

    std::vector<std::string> header{};
    header.reserve(schema.size());
    for (const auto& column : schema.columns()) {
        header.push_back(column.name);
    }
    writeCsvRecord(file, header);
    if (!file) {
        throw StorageError{"failed to write table header for " + tableName};
    }
}

// Validate a row and append its CSV record to the table file.
void FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    schema.validateRow(row);

    std::ofstream file{tablePath(tableName), std::ios::app};
    if (!file) {
        throw StorageError{"failed to append row to " + tableName};
    }

    writeCsvRecord(file, serializeRowForCsv(row));
    if (!file) {
        throw StorageError{"failed to write row to " + tableName};
    }
}

// Check the table header and deserialize all stored rows.
std::vector<Row> FileStorageEngine::readRows(const std::string& tableName, const Schema& schema) const {
    std::ifstream file{tablePath(tableName)};
    if (!file) {
        throw StorageError{"failed to read table storage for " + tableName};
    }

    std::vector<std::string> fields{};
    if (!readCsvRecord(file, fields)) {
        throw StorageError{"table CSV is missing its header: " + tableName};
    }
    if (fields.size() != schema.size()) {
        throw StorageError{"table CSV header width does not match schema: " + tableName};
    }
    for (std::size_t index{0}; index < fields.size(); ++index) {
        const auto id{static_cast<ColumnId>(index)};
        if (fields[index] != schema.column(id).name) {
            throw StorageError{"table CSV header does not match schema: " + tableName};
        }
    }

    std::vector<Row> rows{};
    while (readCsvRecord(file, fields)) {
        rows.push_back(deserializeRowFromCsv(fields, schema));
    }
    return rows;
}

// Delete the table storage file or report a failure.
void FileStorageEngine::dropTable(const std::string& tableName) {
    std::error_code error{};
    const bool removed{std::filesystem::remove(tablePath(tableName), error)};
    if (error || !removed) {
        throw StorageError{"failed to remove table storage for " + tableName};
    }
}

// Build the CSV file path for a table name.
std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / (tableName + ".csv");
}

} // namespace vrdb
