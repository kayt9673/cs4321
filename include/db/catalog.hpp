#pragma once

#include "types/schema.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace vrdb {

struct TableMetadata {
    std::string name;
    Schema schema;
};

class Catalog {
public:
    explicit Catalog(std::filesystem::path databasePath);

    static void validateTableName(const std::string& name);

    void load();
    void createTable(const std::string& name, const Schema& schema);
    void dropTable(const std::string& name);
    bool hasTable(const std::string& name) const;
    const Schema& getSchema(const std::string& name) const;
    std::vector<std::string> listTables() const;

private:
    void persist() const;

    std::filesystem::path path_;
    std::unordered_map<std::string, TableMetadata> tables_;
};

} // namespace vrdb
