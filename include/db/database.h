#pragma once

#include "db/catalog.h"
#include "query/executor.h"
#include "query/query.h"
#include "query/query_result.h"
#include "storage/file_storage_engine.h"
#include "storage/storage_engine.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace vrdb {

class Database {
public:
    explicit Database(const std::filesystem::path& storagePath);

    void createTable(const std::string& name, const Schema& schema);
    void dropTable(const std::string& name);
    RowId insert(const std::string& tableName, const Row& row);
    void update(const std::string& tableName, RowId id, const Row& row);
    void erase(const std::string& tableName, RowId id);
    QueryResult select(const Query& query);

    bool hasTable(const std::string& name) const;
    std::vector<std::string> listTables() const;
    const Schema& getSchema(const std::string& tableName) const;
    std::size_t rowCount(const std::string& tableName) const;

private:
    Catalog catalog_;
    std::unique_ptr<StorageEngine> storage_;
    QueryExecutor executor_;
};

} // namespace vrdb
