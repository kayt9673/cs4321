#include "db/catalog.h"

#include "db/errors.h"
#include "storage/serialization.h"

#include <fstream>
#include <utility>

namespace vrdb {

Catalog::Catalog(std::filesystem::path databasePath)
    : path_(std::move(databasePath) / "catalog.vrdb") {
    std::filesystem::create_directories(path_.parent_path());
}

void Catalog::load() {
    tables_.clear();

    std::ifstream file(path_);
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto separator = line.find('|');
        if (separator == std::string::npos) {
            throw StorageError("malformed catalog entry");
        }

        const std::string tableName = line.substr(0, separator);
        const std::size_t columnCount = static_cast<std::size_t>(std::stoull(line.substr(separator + 1)));

        std::vector<Column> columns;
        columns.reserve(columnCount);
        for (std::size_t i = 0; i < columnCount; ++i) {
            if (!std::getline(file, line)) {
                throw StorageError("catalog ended before table schema was complete");
            }
            columns.push_back(deserializeColumn(line));
        }

        Schema schema(std::move(columns));
        tables_.emplace(tableName, TableMetadata{tableName, std::move(schema)});
    }
}

void Catalog::createTable(const std::string& name, const Schema& schema) {
    if (hasTable(name)) {
        throw DatabaseError("table already exists: " + name);
    }

    tables_.emplace(name, TableMetadata{name, schema});
    persist();
}

void Catalog::dropTable(const std::string& name) {
    if (!hasTable(name)) {
        throw DatabaseError("unknown table: " + name);
    }

    tables_.erase(name);
    persist();
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
    return names;
}

void Catalog::persist() const {
    std::ofstream file(path_, std::ios::trunc);
    if (!file) {
        throw StorageError("unable to write catalog");
    }

    file << "# vrdb catalog v1\n";
    for (const auto& [name, metadata] : tables_) {
        file << name << '|' << metadata.schema.size() << '\n';
        for (const auto& column : metadata.schema.columns()) {
            file << serializeColumn(column) << '\n';
        }
    }
}

} // namespace vrdb
