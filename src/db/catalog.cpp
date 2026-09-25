#include "db/catalog.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace vrdb {
namespace {

const std::vector<std::string> catalogHeader{
    "table_name",
    "column_index",
    "column_name",
    "data_type",
    "vector_dimension",
};

// Parse a nonnegative catalog column index within the supported range.
std::size_t parseColumnIndex(const std::string& value) {
    try {
        return static_cast<std::size_t>(std::stoull(value));;
    } catch (const std::exception&) {
        throw StorageError{"invalid column index in catalog: " + value};
    }
}

} // namespace

// Set the catalog directory and create it if needed.
Catalog::Catalog(std::filesystem::path databasePath)
    : path_{std::move(databasePath) / "catalogs"} {
    std::filesystem::create_directories(path_);
}

// Require a nonempty ASCII identifier safe for use as a table filename.
void Catalog::validateTableName(const std::string& name) {
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

// Load each table schema from its own catalog file.
void Catalog::load() {
    tables_.clear();
    if (std::filesystem::exists(path_.parent_path() / "catalog.csv")) {
        throw StorageError{"shared catalog must be migrated to per-table catalogs"};
    }

    for (const auto& entry : std::filesystem::directory_iterator{path_}) {
        if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
            continue;
        }
        const auto tableName{entry.path().stem().string()};
        validateTableName(tableName);
        std::ifstream file{entry.path()};
        if (!file) {
            throw StorageError{"unable to read catalog for table: " + tableName};
        }
        std::vector<std::string> fields{};
        if (!readCsvRecord(file, fields) || fields != catalogHeader) {
            throw StorageError{"invalid or missing catalog header for table: " + tableName};
        }

        std::vector<std::pair<std::size_t, Column>> indexedColumns{};
        while (readCsvRecord(file, fields)) {
            if (fields.size() != catalogHeader.size() || fields[0] != tableName) {
                throw StorageError{"malformed catalog record for table: " + tableName};
            }
            indexedColumns.emplace_back(
                parseColumnIndex(fields[1]),
                deserializeColumn(fields[2], fields[3], fields[4]));
        }
        if (file.bad()) {
            throw StorageError{"failed to read catalog for table: " + tableName};
        }
        std::sort(indexedColumns.begin(), indexedColumns.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

        std::vector<Column> columns{};
        columns.reserve(indexedColumns.size());
        for (std::size_t expectedIndex{0}; expectedIndex < indexedColumns.size(); ++expectedIndex) {
            if (indexedColumns[expectedIndex].first != expectedIndex) {
                throw StorageError{
                    "catalog has missing or duplicate column indexes for table: " + tableName};
            }
            columns.push_back(std::move(indexedColumns[expectedIndex].second));
        }
        tables_.emplace(tableName, Schema{std::move(columns)});
    }
}

// Register a table schema and write only its catalog.
void Catalog::createTable(const std::string& name, const Schema& schema) {
    validateTableName(name);
    if (hasTable(name)) {
        throw DatabaseError{"table already exists: " + name};
    }

    tables_.emplace(name, schema);
    try {
        writeCatalog(name);
    } catch (...) {
        tables_.erase(name);
        throw;
    }
}

// Delete a table's catalog before removing its in-memory schema.
void Catalog::dropTable(const std::string& name) {
    if (!hasTable(name)) {
        throw DatabaseError{"unknown table: " + name};
    }
    std::error_code error{};
    const bool removed{std::filesystem::remove(catalogPath(name), error)};
    if (error || !removed) {
        throw StorageError{"unable to remove catalog for table: " + name};
    }
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

// Build the catalog file path for a validated table name.
std::filesystem::path Catalog::catalogPath(const std::string& name) const {
    return path_ / (name + ".csv");
}

// write one table's catalog directly.
void Catalog::writeCatalog(const std::string& name) const {
    const auto& schema{tables_.at(name)};
    std::ofstream file{catalogPath(name), std::ios::trunc};
    if (!file) {
        throw StorageError{"unable to write catalog for table: " + name};
    }
    writeCsvRecord(file, catalogHeader);
    for (std::size_t index{0}; index < schema.size(); ++index) {
        const auto& column{schema.column(static_cast<ColumnId>(index))};
        writeCsvRecord(file, {
            name,
            std::to_string(index),
            column.name,
            std::string{columnTypeName(column.type)},
            serializeTypeDimension(column),
        });
    }
    file.close();
    if (!file) {
        throw StorageError{"unable to finish catalog for table: " + name};
    }
}

} // namespace vrdb
