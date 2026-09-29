#include "storage/file_storage_engine.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

namespace vrdb {

// Set the database directory and create its table and catalog directories if needed.
FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_{std::move(rootDirectory)} {
    std::filesystem::create_directories(rootDirectory_ / "tables");
    std::filesystem::create_directories(rootDirectory_ / "catalogs");
}

// Create a table CSV file with its schema header.
void FileStorageEngine::createTable(const std::string& tableName, const Schema& schema) {
    if (std::filesystem::exists(tablePath(tableName)) || std::filesystem::exists(nextIdPath(tableName))) {
        throw StorageError{"table storage already exists for " + tableName};
    }
    std::ofstream file{tablePath(tableName)};
    if (!file) {
        throw StorageError{"failed to create table storage for " + tableName};
    }
    format_.writeTable(file, schema, {});
    file.close();
    if (!file) {
        throw StorageError{"failed to write table header for " + tableName};
    }
    try {
        writeNextId(tableName, 1);
    } catch (...) {
        std::error_code ignored{};
        std::filesystem::remove(tablePath(tableName), ignored);
        throw;
    }
}

// Validate a row and append its CSV record to the table file.
RowId FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    schema.validateRow(row);
    auto data{readTable(tableName, schema)};
    const RowId id{data.legacy ? static_cast<RowId>(data.rows.size()) + 1 : readNextId(tableName)};
    if (id == std::numeric_limits<RowId>::max()) {
        throw StorageError{"RowId space exhausted for " + tableName};
    }
    // Reserve the ID before writing the row; failed writes may skip IDs, never reuse them.
    writeNextId(tableName, id + 1);
    if (data.legacy) {
        data.rows.push_back(StoredRow{id, row});
        writeTable(tableName, schema, data.rows);
        return id;
    }
    std::ofstream file{tablePath(tableName), std::ios::app};
    if (!file) {
        throw StorageError{"failed to append row to " + tableName};
    }
    format_.writeRow(file, StoredRow{id, row});
    file.close();
    if (!file) {
        throw StorageError{"failed to write row to " + tableName};
    }
    return id;
}

bool FileStorageEngine::updateRow(const std::string& tableName, const Schema& schema,
                                  RowId id, const Row& row) {
    schema.validateRow(row);
    auto data{readTable(tableName, schema)};
    const auto found{std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    })};
    if (found == data.rows.end()) {
        return false;
    }
    if (data.legacy) {
        writeNextId(tableName, static_cast<RowId>(data.rows.size()) + 1);
    }
    found->row = row;
    writeTable(tableName, schema, data.rows);
    return true;
}

bool FileStorageEngine::deleteRow(const std::string& tableName, const Schema& schema, RowId id) {
    auto data{readTable(tableName, schema)};
    const auto found{std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    })};
    if (found == data.rows.end()) {
        return false;
    }
    if (data.legacy) {
        writeNextId(tableName, static_cast<RowId>(data.rows.size()) + 1);
    }
    data.rows.erase(found);
    writeTable(tableName, schema, data.rows);
    return true;
}

// Check the table header and deserialize all stored rows.
std::vector<StoredRow> FileStorageEngine::readRows(const std::string& tableName,
                                                   const Schema& schema) const {
    return readTable(tableName, schema).rows;
}

CsvFormat::TableData FileStorageEngine::readTable(const std::string& tableName,
                                                  const Schema& schema) const {
    std::ifstream file{tablePath(tableName)};
    if (!file) {
        throw StorageError{"failed to read table storage for " + tableName};
    }

    auto data{format_.readTable(file, tableName, schema)};
    if (file.bad()) {
        throw StorageError{"failed to read table storage for " + tableName};
    }
    if (!data.legacy) {
        const auto nextId{readNextId(tableName)};
        if (std::any_of(data.rows.begin(), data.rows.end(), [nextId](const StoredRow& row) {
                return row.id >= nextId;
            })) {
            throw StorageError{"RowId counter does not exceed stored IDs for " + tableName};
        }
    }
    return data;
}

void FileStorageEngine::writeTable(const std::string& tableName, const Schema& schema,
                                   const std::vector<StoredRow>& rows) const {
    const auto temporary{tablePath(tableName).string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to rewrite table " + tableName};
    }
    format_.writeTable(file, schema, rows);
    file.close();
    if (!file) {
        throw StorageError{"failed to write updated table " + tableName};
    }
    std::filesystem::rename(temporary, tablePath(tableName));
}

RowId FileStorageEngine::readNextId(const std::string& tableName) const {
    std::ifstream file{nextIdPath(tableName)};
    if (!file) {
        throw StorageError{"missing RowId counter for " + tableName};
    }
    return format_.readNextId(file);
}

void FileStorageEngine::writeNextId(const std::string& tableName, RowId id) const {
    const auto temporary{nextIdPath(tableName).string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to write RowId counter for " + tableName};
    }
    format_.writeNextId(file, id);
    file.close();
    if (!file) {
        throw StorageError{"failed to write RowId counter for " + tableName};
    }
    std::filesystem::rename(temporary, nextIdPath(tableName));
}

// Delete the table storage file or report a failure.
void FileStorageEngine::dropTable(const std::string& tableName) {
    std::error_code error{};
    const bool removed{std::filesystem::remove(tablePath(tableName), error)};
    if (error || !removed) {
        throw StorageError{"failed to remove table storage for " + tableName};
    }
    std::filesystem::remove(nextIdPath(tableName), error);
    if (error) {
        throw StorageError{"failed to remove RowId counter for " + tableName};
    }
}

// Build the CSV file path for a table name.
std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / "tables" / (tableName + format_.extension);
}

std::filesystem::path FileStorageEngine::nextIdPath(const std::string& tableName) const {
    return rootDirectory_ / "tables" / (tableName + ".nextid");
}

// Build the catalog file path for a validated table name.
std::filesystem::path FileStorageEngine::catalogPath(const std::string& tableName) const {
    return rootDirectory_ / "catalogs" / (tableName + format_.extension);
}

std::vector<std::string> FileStorageEngine::listTables() const {
    std::vector<std::string> names{};
    for (const auto& entry : std::filesystem::directory_iterator{rootDirectory_ / "catalogs"}) {
        if (entry.is_regular_file() && entry.path().extension() == format_.extension) {
            names.push_back(entry.path().stem().string());
        }
    }
    return names;
}

Schema FileStorageEngine::readSchema(const std::string& tableName) const {
    std::ifstream file{catalogPath(tableName)};
    if (!file) {
        throw StorageError{"unable to read catalog for table: " + tableName};
    }
    auto schema{format_.readSchema(file, tableName)};
    if (file.bad()) {
        throw StorageError{"failed to read catalog for " + tableName};
    }
    return schema;
}

void FileStorageEngine::writeSchema(const std::string& tableName, const Schema& schema) {
    const auto path{catalogPath(tableName)};
    if (std::filesystem::exists(path)) {
        throw StorageError{"catalog already exists for " + tableName};
    }
    const auto temporary{path.string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to write catalog for " + tableName};
    }
    format_.writeSchema(file, tableName, schema);
    file.close();
    if (!file) {
        throw StorageError{"failed to finish catalog for " + tableName};
    }
    std::filesystem::rename(temporary, path);
}

void FileStorageEngine::removeSchema(const std::string& tableName) {
    const auto path{catalogPath(tableName)};
    std::error_code error{};
    if (!std::filesystem::remove(path, error) || error) {
        throw StorageError{"failed to remove catalog " + path.string()};
    }
}

} // namespace vrdb
