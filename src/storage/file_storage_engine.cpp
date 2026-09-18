#include "storage/file_storage_engine.h"

#include "db/errors.h"
#include "storage/serialization.h"

#include <fstream>
#include <utility>

namespace vrdb {

FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {
    std::filesystem::create_directories(rootDirectory_);
}

void FileStorageEngine::createTable(const std::string& tableName) {
    std::filesystem::create_directories(rootDirectory_);

    std::ofstream file(tablePath(tableName), std::ios::trunc);
    if (!file) {
        throw StorageError("failed to create table storage for " + tableName);
    }

    file << "# vrdb table rows v1 " << tableName << '\n';
}

void FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    row.validateAgainst(schema);

    std::ofstream file(tablePath(tableName), std::ios::app);
    if (!file) {
        throw StorageError("failed to append row to " + tableName);
    }

    file << serializeRow(row) << '\n';
}

std::vector<Row> FileStorageEngine::readRows(const std::string& tableName, const Schema& schema) const {
    std::ifstream file(tablePath(tableName));
    if (!file) {
        throw StorageError("failed to read table storage for " + tableName);
    }

    std::vector<Row> rows;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        rows.push_back(deserializeRow(line, schema));
    }

    return rows;
}

void FileStorageEngine::dropTable(const std::string& tableName) {
    std::filesystem::remove(tablePath(tableName));
}

std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / (tableName + ".vrdb");
}

} // namespace vrdb
