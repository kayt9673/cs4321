#include "db/database_manager.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

// Open database storage and load the persisted catalog.
DatabaseManager::DatabaseManager(const std::filesystem::path& storagePath)
    : storage_{storagePath} {
    catalog_.load(storage_);
}

// Create a table and persist its schema and storage.
void DatabaseManager::createTable(const std::string& name, const Schema& schema) {
    Catalog::validateString(name);
    if (catalog_.hasTable(name)) {
        throw DatabaseError{"table already exists: " + name};
    }
    storage_.createTable(name, schema);
    try {
        catalog_.createTable(name, schema, storage_);
    } catch (...) {
        storage_.dropTable(name);
        throw;
    }
}

// Remove a table from storage and the catalog.
void DatabaseManager::dropTable(const std::string& name) {
    if (!catalog_.hasTable(name)) {
        throw DatabaseError{"unknown table: " + name};
    }
    storage_.dropTable(name);
    catalog_.dropTable(name, storage_);
}

// Validate and append a row to the named table.
RowId DatabaseManager::insert(const std::string& tableName, const Row& row) {
    const auto& schema{catalog_.getSchema(tableName)};
    return storage_.appendRow(tableName, schema, row);
}

void DatabaseManager::update(const std::string& tableName, RowId id, const Row& row) {
    const auto& schema{catalog_.getSchema(tableName)};
    if (!storage_.updateRow(tableName, schema, id, row)) {
        throw DatabaseError{"unknown RowId " + std::to_string(id) + " in table " + tableName};
    }
}

void DatabaseManager::erase(const std::string& tableName, RowId id) {
    const auto& schema{catalog_.getSchema(tableName)};
    if (!storage_.deleteRow(tableName, schema, id)) {
        throw DatabaseError{"unknown RowId " + std::to_string(id) + " in table " + tableName};
    }
}

// Read a table and execute the requested query against its rows.
QueryResult DatabaseManager::select(const Query& query) {
    const auto& schema{catalog_.getSchema(query.table)};
    return executor_.execute(query, schema, storage_.readRows(query.table, schema));
}

// Return whether the catalog contains the named table.
bool DatabaseManager::hasTable(const std::string& name) const {
    return catalog_.hasTable(name);
}

// Return table names in sorted order.
std::vector<std::string> DatabaseManager::listTables() const {
    return catalog_.listTables();
}

// Return the named table schema or report an unknown table.
const Schema& DatabaseManager::getSchema(const std::string& tableName) const {
    return catalog_.getSchema(tableName);
}

// Read the table and return its number of stored rows.
std::size_t DatabaseManager::rowCount(const std::string& tableName) const {
    const auto& schema{catalog_.getSchema(tableName)};
    return storage_.readRows(tableName, schema).size();
}

} // namespace vrdb
