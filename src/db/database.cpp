#include "db/database.h"

#include <stdexcept>
#include <utility>

namespace vrdb {

Database::Database(std::filesystem::path storagePath)
    : storageEngine_(std::make_unique<FileStorageEngine>(std::move(storagePath))) {}

Database::Database(std::unique_ptr<StorageEngine> storageEngine)
    : storageEngine_(std::move(storageEngine)) {
    if (!storageEngine_) {
        throw std::invalid_argument("storage engine cannot be null");
    }
}

void Database::createTable(const std::string& name, const Schema& schema) {
    if (tables_.find(name) != tables_.end()) {
        throw std::invalid_argument("table already exists: " + name);
    }

    storageEngine_->createTable(name, schema);
    tables_.emplace(name, Table(name, schema));
}

void Database::insert(const std::string& tableName, Row row) {
    Table& table = mutableTable(tableName);
    table.schema().validateRow(row);
    storageEngine_->appendRow(tableName, table.schema(), row);
    table.addRow(std::move(row));
}

void Database::insert(const std::string& tableName, std::vector<Value> values) {
    insert(tableName, Row(std::move(values)));
}

std::vector<Row> Database::rows(const std::string& tableName) const {
    const Table& table = this->table(tableName);
    return storageEngine_->readRows(tableName, table.schema());
}

const Table& Database::table(const std::string& tableName) const {
    const auto found = tables_.find(tableName);
    if (found == tables_.end()) {
        throw std::out_of_range("unknown table: " + tableName);
    }
    return found->second;
}

Table& Database::mutableTable(const std::string& tableName) {
    const auto found = tables_.find(tableName);
    if (found == tables_.end()) {
        throw std::out_of_range("unknown table: " + tableName);
    }
    return found->second;
}

} // namespace vrdb
