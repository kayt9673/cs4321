#pragma once

#include "db/catalog.hpp"
#include "query/executor.hpp"
#include "query/query.hpp"
#include "storage/file_storage_engine.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace vrdb {

class DatabaseManager {
public:
    // Open database storage and load the persisted catalog.
    explicit DatabaseManager(const std::filesystem::path& storagePath);

    // Create a table and persist its schema and storage.
    void createTable(const std::string& name, const Schema& schema);
    // Remove a table from storage and the catalog.
    void dropTable(const std::string& name);
    // Validate and append a row to the named table.
    void insert(const std::string& tableName, const Row& row);
    // Read a table and execute the requested query against its rows.
    QueryResult select(const Query& query);

    // Return whether the catalog contains the named table.
    bool hasTable(const std::string& name) const;
    // Return table names in sorted order.
    std::vector<std::string> listTables() const;
    // Return the named table schema or report an unknown table.
    const Schema& getSchema(const std::string& tableName) const;
    // Read the table and return its number of stored rows.
    std::size_t rowCount(const std::string& tableName) const;

private:
    Catalog catalog_;
    FileStorageEngine storage_;
    QueryExecutor executor_;
};

} // namespace vrdb
