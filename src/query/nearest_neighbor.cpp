#include "db/database_manager.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <queue>

namespace vrdb {
namespace {

bool nearer(const VectorCandidate& a, const VectorCandidate& b) {
    return a.distance < b.distance || (a.distance == b.distance && a.id < b.id);
}

struct RankedRow {
    VectorCandidate candidate;
    Row row;
};

struct NearerRow {
    bool operator()(const RankedRow& a, const RankedRow& b) const {
        return nearer(a.candidate, b.candidate);
    }
};

} // namespace

void DatabaseManager::createVectorIndex(const VectorIndexDefinition& definition) {
    indexes_.create(definition, catalog_, storage_);
}

void DatabaseManager::dropVectorIndex(const std::string& name) {
    indexes_.drop(name, storage_);
}

std::vector<VectorIndexDefinition> DatabaseManager::listVectorIndexes() const {
    return indexes_.list();
}

NearestNeighborResult DatabaseManager::nearestNeighbors(const NearestNeighborQuery& query) {
    const auto& schema{catalog_.getSchema(query.table)};
    const auto column{schema.columnId(query.column)};
    if (!column || !isVector(schema.column(*column).type)) {
        throw QueryError{"nearest-neighbor search requires a vector column: " + query.column};
    }
    if (query.mode != NearestNeighborMode::AUTO && query.mode != NearestNeighborMode::EXACT) {
        throw QueryError{"unknown nearest-neighbor execution mode"};
    }
    const auto context{query.table + "." + query.column};
    validateSearchVector(query.referenceVector, schema.column(*column).vectorDimension,
                         query.metric, "nearest-neighbor query on " + context);
    const Query filter{query.table, query.projection, query.predicates, std::nullopt, 0};
    const auto prepared{executor_.prepare(filter, schema)};
    NearestNeighborResult result{
        QueryResult{executor_.projectSchema(schema, prepared.projection), {}, {}}, {},
        NearestNeighborMethod::EXACT};
    const auto* index{query.mode == NearestNeighborMode::AUTO && query.predicates.empty() ?
        indexes_.find(query.table, query.column, query.metric) : nullptr};
    if (index) result.method = NearestNeighborMethod::HNSW;
    if (query.k == 0) return result;

    if (index) {
        auto candidates{indexes_.search(*index, schema, storage_, query.referenceVector, query.k)};
        for (auto& candidate : candidates) {
            const auto& row{indexes_.row(query.table, candidate.id)};
            candidate.distance = distance(std::get<std::vector<float>>(row.cell(*column)),
                                          query.referenceVector, query.metric);
        }
        std::sort(candidates.begin(), candidates.end(), nearer);
        for (const auto& candidate : candidates) {
            result.result.rowIds.push_back(candidate.id);
            result.result.rows.push_back(executor_.projectRow(indexes_.row(query.table, candidate.id),
                                                              prepared.projection));
            result.distances.push_back(candidate.distance);
        }
    } else {
        std::priority_queue<RankedRow, std::vector<RankedRow>, NearerRow> best;
        auto cursor{storage_.openTableCursor(query.table, schema)};
        while (!cursor.finished()) {
            for (auto& stored : cursor.readNextPage(1024)->takeRows()) {
                if (!executor_.matches(prepared, stored.row) || isNullCell(stored.row.cell(*column))) continue;
                const auto& values{std::get<std::vector<float>>(stored.row.cell(*column))};
                validateSearchVector(values, query.referenceVector.size(), query.metric,
                    context + " RowId " + std::to_string(stored.id));
                const VectorCandidate candidate{stored.id, distance(values, query.referenceVector, query.metric)};
                if (best.size() < query.k || nearer(candidate, best.top().candidate)) {
                    if (best.size() == query.k) best.pop();
                    best.push({candidate, std::move(stored.row)});
                }
            }
        }
        std::vector<RankedRow> ordered;
        ordered.reserve(best.size());
        while (!best.empty()) {
            ordered.push_back(best.top());
            best.pop();
        }
        std::reverse(ordered.begin(), ordered.end());
        for (const auto& item : ordered) {
            result.result.rowIds.push_back(item.candidate.id);
            result.result.rows.push_back(executor_.projectRow(item.row, prepared.projection));
            result.distances.push_back(item.candidate.distance);
        }
    }
    return result;
}

} // namespace vrdb
