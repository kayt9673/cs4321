#include "db/database.h"

#include "db/errors.h"

#include <utility>

namespace vrdb {

Database::Database(const std::filesystem::path& storagePath)
    : catalog_(storagePath),
      storage_(std::make_unique<FileStorageEngine>(storagePath / "tables")) {
    catalog_.load();
}

void Database::createTable(const std::string& name, const Schema& schema) {
    Catalog::validateTableName(name);
    if (catalog_.hasTable(name)) {
        throw DatabaseError("table already exists: " + name);
    }
    storage_->createTable(name, schema);
    try {
        catalog_.createTable(name, schema);
    } catch (...) {
        storage_->dropTable(name);
        throw;
    }
}

void Database::dropTable(const std::string& name) {
    if (!catalog_.hasTable(name)) {
        throw DatabaseError("unknown table: " + name);
    }
    storage_->dropTable(name);
    catalog_.dropTable(name);
}

void Database::insert(const std::string& tableName, const Row& row) {
    const auto& schema = catalog_.getSchema(tableName);
    schema.validateRow(row);
    storage_->appendRow(tableName, schema, row);
}

QueryResult Database::select(const Query& query) {
    const auto& schema = catalog_.getSchema(query.table);
    return executor_.execute(query, schema, storage_->readRows(query.table, schema));
}

bool Database::hasTable(const std::string& name) const {
    return catalog_.hasTable(name);
}

std::vector<std::string> Database::listTables() const {
    return catalog_.listTables();
}

const Schema& Database::getSchema(const std::string& tableName) const {
    return catalog_.getSchema(tableName);
}

std::size_t Database::rowCount(const std::string& tableName) const {
    const auto& schema = catalog_.getSchema(tableName);
    return storage_->readRows(tableName, schema).size();
}

} // namespace vrdb
