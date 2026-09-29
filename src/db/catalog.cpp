#include "db/catalog.hpp"

#include "db/errors.hpp"
#include "storage/storage_engine.hpp"

#include <algorithm>
#include <string_view>

namespace vrdb {

// Require a nonempty ASCII identifier safe for use as a table filename.
void Catalog::validateString(const std::string& name) {
    if (name.empty()) {
        throw DatabaseError{"table name cannot be empty"};
    }

    constexpr std::string_view allowed{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"};
    if ((name.front() >= '0' && name.front() <= '9') ||
        name.find_first_not_of(allowed) != std::string::npos) {
        throw DatabaseError{
            "invalid table name '" + name +
            "': use letters, digits, and underscores, starting with a letter or underscore"};
    }
}

// Load each table schema from storage.
void Catalog::load(const StorageEngine& storage) {
    tables_.clear();
    for (const auto& tableName : storage.listTables()) {
        validateString(tableName);
        tables_.emplace(tableName, storage.readSchema(tableName));
    }
}

// Register a table schema and write only its catalog.
void Catalog::createTable(const std::string& name, const Schema& schema, StorageEngine& storage) {
    validateString(name);
    if (hasTable(name)) {
        throw DatabaseError{"table already exists: " + name};
    }

    tables_.emplace(name, schema);
    try {
        storage.writeSchema(name, schema);
    } catch (...) {
        tables_.erase(name);
        throw;
    }
}

// Delete a table's catalog before removing its in-memory schema.
void Catalog::dropTable(const std::string& name, StorageEngine& storage) {
    if (!hasTable(name)) {
        throw DatabaseError{"unknown table: " + name};
    }
    storage.removeSchema(name);
    tables_.erase(name);
}

// Return whether the catalog contains the named table.
bool Catalog::hasTable(const std::string& name) const {
    return tables_.find(name) != tables_.end();
}

// Return the named table schema or report an unknown table.
const Schema& Catalog::getSchema(const std::string& name) const {
    const auto found{tables_.find(name)};
    if (found == tables_.end()) {
        throw DatabaseError{"unknown table: " + name};
    }
    return found->second;
}

// Return table names in sorted order.
std::vector<std::string> Catalog::listTables() const {
    std::vector<std::string> names{};
    names.reserve(tables_.size());
    for (const auto& table : tables_) {
        names.push_back(table.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace vrdb
