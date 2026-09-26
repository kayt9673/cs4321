#pragma once

#include "types/schema.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace vrdb {

class Catalog {
public:
    // Set the catalog directory and create it if needed.
    explicit Catalog(std::filesystem::path databasePath);

    // Require a nonempty ASCII identifier safe for use as a table filename.
    static void validateString(const std::string& name);

    // Load each table schema from its own catalog file.
    void load();
    // Register a table schema and write only its catalog.
    void createTable(const std::string& name, const Schema& schema);
    // Delete a table's catalog before removing its in-memory schema.
    void dropTable(const std::string& name);
    // Return whether the catalog contains the named table.
    bool hasTable(const std::string& name) const;
    // Return the named table schema or report an unknown table.
    const Schema& getSchema(const std::string& name) const;
    // Return table names in sorted order.
    std::vector<std::string> listTables() const;

private:
    // Build the catalog file path for a validated table name.
    std::filesystem::path catalogPath(const std::string& name) const;
    // write one table's catalog directly.
    void writeCatalog(const std::string& name) const;

    std::filesystem::path path_;
    std::unordered_map<std::string, Schema> tables_;
};

} // namespace vrdb
