#include "db/index_manager.hpp"

#include "db/errors.hpp"

#include <algorithm>

namespace vrdb {

void IndexManager::validateDefinition(VectorIndexDefinition& definition, const Catalog& catalog) {
    Catalog::validateString(definition.name);
    Catalog::validateString(definition.table);
    validateHnswOptions(definition.options);
    const auto& column{catalog.getSchema(definition.table).column(definition.column)};
    if (!isVector(column.type)) throw SchemaError{"index requires a vector column: " + definition.column};
    if (definition.dimension != 0 && definition.dimension != column.vectorDimension) {
        throw SchemaError{"index dimension differs from schema: " + definition.name};
    }
    definition.dimension = column.vectorDimension;
    if (definition.metric != DistanceMetric::EUCLIDEAN && definition.metric != DistanceMetric::COSINE) {
        throw QueryError{"unknown index distance metric"};
    }
}

void IndexManager::load(const Catalog& catalog, const FileStorageEngine& storage) {
    std::map<std::string, VectorIndexDefinition> next;
    for (auto definition : storage.readIndexDefinitions()) {
        try {
            validateDefinition(definition, catalog);
            for (const auto& [name, existing] : next) {
                if (existing.table == definition.table && existing.column == definition.column &&
                    existing.metric == definition.metric) {
                    throw StorageError{"duplicate vector index target"};
                }
            }
            const auto name{definition.name};
            if (!next.emplace(name, std::move(definition)).second) {
                throw StorageError{"duplicate vector index name"};
            }
        } catch (const DatabaseError& error) {
            throw StorageError{"invalid vector index definition: " + std::string{error.what()}};
        }
    }
    definitions_.swap(next);
    graphs_.clear();
    rows_.clear();
}

void IndexManager::create(VectorIndexDefinition definition, const Catalog& catalog,
                           FileStorageEngine& storage) {
    validateDefinition(definition, catalog);
    if (definitions_.contains(definition.name)) {
        throw DatabaseError{"index already exists: " + definition.name};
    }
    if (find(definition.table, definition.column, definition.metric)) {
        throw DatabaseError{"index already exists for table, column, and metric"};
    }
    const auto name{definition.name};
    definitions_.emplace(name, definition);
    try {
        storage.writeIndexDefinition(definition);
    } catch (...) {
        definitions_.erase(name);
        throw;
    }
}

void IndexManager::drop(const std::string& name, FileStorageEngine& storage) {
    const auto found{definitions_.find(name)};
    if (found == definitions_.end()) throw DatabaseError{"unknown vector index: " + name};
    const auto table{found->second.table};
    storage.removeIndexDefinition(name);
    graphs_.erase(name);
    definitions_.erase(found);
    const bool stillIndexed{std::any_of(definitions_.begin(), definitions_.end(), [&](const auto& entry) {
        return entry.second.table == table;
    })};
    if (!stillIndexed) rows_.erase(table);
}

void IndexManager::dropTable(const std::string& table, FileStorageEngine& storage) {
    invalidate(table);
    // Remove definitions before table storage, so a failed removal cannot leave
    // definitions referring to an already deleted table.
    for (const auto& definition : list()) {
        if (definition.table == table) drop(definition.name, storage);
    }
}

std::vector<VectorIndexDefinition> IndexManager::list() const {
    std::vector<VectorIndexDefinition> result;
    for (const auto& [name, definition] : definitions_) result.push_back(definition);
    return result;
}

void IndexManager::invalidate(const std::string& table) {
    for (const auto& [name, definition] : definitions_) {
        if (definition.table == table) graphs_.erase(name);
    }
    rows_.erase(table);
}

const VectorIndexDefinition* IndexManager::find(const std::string& table, const std::string& column,
                                                DistanceMetric metric) const {
    for (const auto& [name, definition] : definitions_) {
        if (definition.table == table && definition.column == column && definition.metric == metric) {
            return &definition;
        }
    }
    return nullptr;
}

std::vector<VectorCandidate> IndexManager::search(const VectorIndexDefinition& definition,
    const Schema& schema, const FileStorageEngine& storage, std::span<const float> query, std::size_t k) {
    try {
        auto graph{graphs_.find(definition.name)};
        if (graph == graphs_.end()) {
            auto cached{rows_.find(definition.table)};
            RowCache pending;
            if (cached == rows_.end()) {
                auto cursor{storage.openTableCursor(definition.table, schema)};
                while (!cursor.finished()) {
                    for (auto& stored : cursor.readNextPage(1024)->takeRows()) {
                        if (!pending.emplace(stored.id, std::move(stored.row)).second) {
                            throw StorageError{"duplicate RowId in indexed table"};
                        }
                    }
                }
            }
            const auto& cache{cached == rows_.end() ? pending : cached->second};
            const auto column{schema.columnId(definition.column).value()};
            std::vector<VectorEntry> entries;
            entries.reserve(cache.size());
            for (const auto& [id, row] : cache) {
                if (isNullCell(row.cell(column))) continue;
                entries.push_back({id, std::get<std::vector<float>>(row.cell(column))});
            }
            auto built{std::make_unique<HnswVectorIndex>(definition.dimension, definition.metric, definition.options)};
            built->build(entries);
            if (cached == rows_.end()) rows_.emplace(definition.table, std::move(pending));
            graph = graphs_.emplace(definition.name, std::move(built)).first;
        }
        return graph->second->search(query, k);
    } catch (const DatabaseError& error) {
        throw QueryError{"index '" + definition.name + "' on " + definition.table + "." +
                         definition.column + ": " + error.what()};
    }
}

const Row& IndexManager::row(const std::string& table, RowId id) const {
    return rows_.at(table).at(id);
}

} // namespace vrdb
