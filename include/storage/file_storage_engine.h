#pragma once

#include "storage/storage_engine.h"

#include <filesystem>
#include <string>

namespace vrdb {

class FileStorageEngine : public StorageEngine {
public:
    explicit FileStorageEngine(std::filesystem::path rootDirectory);

    void createTable(const std::string& tableName, const Schema& schema) override;
    RowId appendRow(const std::string& tableName, const Schema& schema, const Row& row) override;
    bool updateRow(const std::string& tableName, const Schema& schema, RowId id, const Row& row) override;
    bool deleteRow(const std::string& tableName, const Schema& schema, RowId id) override;
    std::vector<StoredRow> readRows(const std::string& tableName, const Schema& schema) const override;
    void dropTable(const std::string& tableName) override;

private:
    struct TableData {
        std::vector<StoredRow> rows;
        bool legacy;
    };

    TableData readTable(const std::string& tableName, const Schema& schema) const;
    void writeTable(const std::string& tableName, const Schema& schema, const std::vector<StoredRow>& rows) const;
    RowId readNextId(const std::string& tableName) const;
    void writeNextId(const std::string& tableName, RowId id) const;
    std::filesystem::path tablePath(const std::string& tableName) const;
    std::filesystem::path nextIdPath(const std::string& tableName) const;

    std::filesystem::path rootDirectory_;
};

} // namespace vrdb
