#include "db/catalog.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <string_view>
#include <utility>

namespace vrdb {
namespace {

const std::vector<std::string> catalogHeader{
    "format_version",
    "table_name",
    "column_index",
    "column_name",
    "data_type",
    "vector_dimension",
};

constexpr std::string_view catalogFormatVersion = "1";

std::size_t parseColumnIndex(const std::string& value) {
    try {
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            })) {
            throw StorageError("invalid column index in catalog: " + value);
        }
        std::size_t parsed = 0;
        const auto index = std::stoull(value, &parsed);
        if (parsed != value.size() || index > std::numeric_limits<std::size_t>::max()) {
            throw StorageError("invalid column index in catalog: " + value);
        }
        return static_cast<std::size_t>(index);
    } catch (const DatabaseError&) {
        throw;
    } catch (const std::exception&) {
        throw StorageError("invalid column index in catalog: " + value);
    }
}

} // namespace

Catalog::Catalog(std::filesystem::path databasePath)
    : path_(std::move(databasePath) / "catalog.csv") {
    std::filesystem::create_directories(path_.parent_path());
}

void Catalog::validateTableName(const std::string& name) {
    if (name.empty()) {
        throw DatabaseError("table name cannot be empty");
    }

    const auto isAsciiLetter = [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z');
    };
    const auto validFirst = [&](unsigned char character) {
        return isAsciiLetter(character) || character == '_';
    };
    const auto validRest = [&](unsigned char character) {
        return isAsciiLetter(character) ||
            (character >= '0' && character <= '9') || character == '_';
    };

    if (!validFirst(static_cast<unsigned char>(name.front())) ||
        !std::all_of(name.begin() + 1, name.end(), [&](char character) {
            return validRest(static_cast<unsigned char>(character));
        })) {
        throw DatabaseError(
            "invalid table name '" + name +
            "': use letters, digits, and underscores, starting with a letter or underscore");
    }
}

void Catalog::load() {
    tables_.clear();

    std::ifstream file(path_);
    if (!file) {
        persist();
        return;
    }

    std::vector<std::string> fields;
    if (!readCsvRecord(file, fields) || fields != catalogHeader) {
        throw StorageError("catalog.csv has an invalid or missing header");
    }

    std::map<std::string, std::vector<std::pair<std::size_t, Column>>> pendingTables;
    while (readCsvRecord(file, fields)) {
        if (fields.size() != catalogHeader.size()) {
            throw StorageError("catalog.csv contains a malformed record");
        }

        if (fields[0] != catalogFormatVersion) {
            throw StorageError("unsupported catalog format version: " + fields[0]);
        }

        const auto& tableName = fields[1];
        try {
            validateTableName(tableName);
        } catch (const DatabaseError& error) {
            throw StorageError("catalog.csv contains " + std::string(error.what()));
        }

        const auto columnIndex = parseColumnIndex(fields[2]);
        auto column = deserializeColumn(fields[3], fields[4], fields[5]);
        pendingTables[tableName].emplace_back(
            columnIndex,
            std::move(column));
    }

    for (auto& [tableName, indexedColumns] : pendingTables) {
        std::sort(indexedColumns.begin(), indexedColumns.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

        std::vector<Column> columns;
        columns.reserve(indexedColumns.size());
        for (std::size_t expectedIndex = 0; expectedIndex < indexedColumns.size(); ++expectedIndex) {
            if (indexedColumns[expectedIndex].first != expectedIndex) {
                throw StorageError(
                    "catalog.csv has missing or duplicate column indexes for table: " + tableName);
            }
            columns.push_back(std::move(indexedColumns[expectedIndex].second));
        }

        Schema schema(std::move(columns));
        tables_.emplace(tableName, TableMetadata{tableName, std::move(schema)});
    }
}

void Catalog::createTable(const std::string& name, const Schema& schema) {
    validateTableName(name);
    if (hasTable(name)) {
        throw DatabaseError("table already exists: " + name);
    }

    tables_.emplace(name, TableMetadata{name, schema});
    try {
        persist();
    } catch (...) {
        tables_.erase(name);
        throw;
    }
}

void Catalog::dropTable(const std::string& name) {
    if (!hasTable(name)) {
        throw DatabaseError("unknown table: " + name);
    }

    const auto metadata = tables_.at(name);
    tables_.erase(name);
    try {
        persist();
    } catch (...) {
        tables_.emplace(name, metadata);
        throw;
    }
}

bool Catalog::hasTable(const std::string& name) const {
    return tables_.find(name) != tables_.end();
}

const Schema& Catalog::getSchema(const std::string& name) const {
    const auto found = tables_.find(name);
    if (found == tables_.end()) {
        throw DatabaseError("unknown table: " + name);
    }
    return found->second.schema;
}

std::vector<std::string> Catalog::listTables() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& [name, metadata] : tables_) {
        (void)metadata;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void Catalog::persist() const {
    const auto temporaryPath = path_.string() + ".tmp";
    {
        std::ofstream file(temporaryPath, std::ios::trunc);
        if (!file) {
            throw StorageError("unable to write temporary catalog");
        }

        writeCsvRecord(file, catalogHeader);
        for (const auto& tableName : listTables()) {
            const auto& schema = tables_.at(tableName).schema;
            for (std::size_t index = 0; index < schema.size(); ++index) {
                const auto& column = schema.column(static_cast<ColumnId>(index));
                writeCsvRecord(file, {
                    std::string(catalogFormatVersion),
                    tableName,
                    std::to_string(index),
                    column.name,
                    std::string(columnTypeName(column.type)),
                    serializeTypeDimension(column),
                });
            }
        }
        file.flush();
        if (!file) {
            throw StorageError("unable to flush temporary catalog");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporaryPath, path_, error);
    if (error) {
        std::filesystem::remove(temporaryPath);
        throw StorageError("unable to replace catalog: " + error.message());
    }
}

} // namespace vrdb
