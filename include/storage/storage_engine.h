#pragma once

#include "types/row.h"
#include "types/schema.h"

#include <filesystem>
#include <string>
#include <vector>

namespace vrdb {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual void createTable(const std::string& tableName, const Schema& schema) = 0;
    virtual void appendRow(const std::string& tableName, const Schema& schema, const Row& row) = 0;
    virtual std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const = 0;
};

class FileStorageEngine : public StorageEngine {
public:
    explicit FileStorageEngine(std::filesystem::path rootDirectory);

    void createTable(const std::string& tableName, const Schema& schema) override;
    void appendRow(const std::string& tableName, const Schema& schema, const Row& row) override;
    std::vector<Row> readRows(const std::string& tableName, const Schema& schema) const override;

private:
    std::filesystem::path tablePath(const std::string& tableName) const;

    std::filesystem::path rootDirectory_;
};

} // namespace vrdb
