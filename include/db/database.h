#pragma once

#include "storage/storage_engine.h"
#include "storage/table.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vrdb {

class Database {
public:
    explicit Database(std::filesystem::path storagePath);
    explicit Database(std::unique_ptr<StorageEngine> storageEngine);

    void createTable(const std::string& name, const Schema& schema);
    void insert(const std::string& tableName, Row row);
    void insert(const std::string& tableName, std::vector<Value> values);
    std::vector<Row> rows(const std::string& tableName) const;

    const Table& table(const std::string& tableName) const;

private:
    Table& mutableTable(const std::string& tableName);

    std::unique_ptr<StorageEngine> storageEngine_;
    std::unordered_map<std::string, Table> tables_;
};

} // namespace vrdb
