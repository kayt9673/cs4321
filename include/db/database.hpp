#pragma once

#include "db/catalog.hpp"
#include "query/executor.hpp"
#include "query/query.hpp"
#include "query/query_result.hpp"
#include "storage/file_storage_engine.hpp"
#include "storage/storage_engine.hpp"

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
    void insert(const std::string& tableName, const Row& row);
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
