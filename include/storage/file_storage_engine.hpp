#pragma once

#include "storage/storage_engine.hpp"

#include <filesystem>
#include <string>

namespace vrdb {

class FileStorageEngine : public StorageEngine {
public:
    explicit FileStorageEngine(std::filesystem::path rootDirectory);

    void createTable(const std::string& tableName, const Schema& schema) override;
    void appendRow(const std::string& tableName, const Schema& schema, const Row& row) override;
    std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const override;
    void dropTable(const std::string& tableName) override;

private:
    std::filesystem::path tablePath(const std::string& tableName) const;

    std::filesystem::path rootDirectory_;
};

} // namespace vrdb
