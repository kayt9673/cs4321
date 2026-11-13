#pragma once

#include "db/catalog.hpp"
#include "storage/file_storage_engine.hpp"
#include "vector/index.hpp"

#include <map>
#include <memory>
#include <unordered_map>

namespace vrdb {

// Value-owned state, with no references back to the owning DatabaseManager.
// Calls are externally serialized, like the rest of DatabaseManager.
class IndexManager {
public:
    void load(const Catalog& catalog, const FileStorageEngine& storage);
    void create(VectorIndexDefinition definition, const Catalog& catalog,
                FileStorageEngine& storage);
    void drop(const std::string& name, FileStorageEngine& storage);
    void dropTable(const std::string& table, FileStorageEngine& storage);
    std::vector<VectorIndexDefinition> list() const;
    void invalidate(const std::string& table);
    const VectorIndexDefinition* find(const std::string& table, const std::string& column,
                                      DistanceMetric metric) const;
    std::vector<VectorCandidate> search(const VectorIndexDefinition& definition,
                                        const Schema& schema, const FileStorageEngine& storage,
                                        std::span<const float> query, std::size_t k);
    const Row& row(const std::string& table, RowId id) const;

private:
    using RowCache = std::unordered_map<RowId, Row>;
    static void validateDefinition(VectorIndexDefinition& definition, const Catalog& catalog);
    std::map<std::string, VectorIndexDefinition> definitions_;
    std::unordered_map<std::string, std::unique_ptr<VectorIndex>> graphs_;
    std::unordered_map<std::string, RowCache> rows_;
};

} // namespace vrdb
