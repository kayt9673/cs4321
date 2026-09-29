#pragma once

#include "types/schema.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace vrdb {

class StorageEngine;

class Catalog {
public:
    // Require a nonempty ASCII identifier safe for use as a table filename.
    static void validateString(const std::string& name);

    // Load each table schema from storage.
    void load(const StorageEngine& storage);
    // Register a table schema and write only its catalog.
    void createTable(const std::string& name, const Schema& schema, StorageEngine& storage);
    // Delete a table's catalog before removing its in-memory schema.
    void dropTable(const std::string& name, StorageEngine& storage);
    // Return whether the catalog contains the named table.
    bool hasTable(const std::string& name) const;
    // Return the named table schema or report an unknown table.
    const Schema& getSchema(const std::string& name) const;
    // Return table names in sorted order.
    std::vector<std::string> listTables() const;

private:
    std::unordered_map<std::string, Schema> tables_;
};

} // namespace vrdb
