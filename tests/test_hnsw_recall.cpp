#include "hnsw_test_support.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

int main() {
    using namespace vrdb;
    hnsw_test::TemporaryDirectory temporary;
    constexpr std::size_t count{10000}, dimension{16}, queryCount{100}, k{10};
    const Schema schema{{Column{"v", DataType::VECTOR, dimension}}};
    DatabaseManager db{temporary.path};
    db.createTable("vectors", schema);
    std::mt19937 random{4321};
    // Convert generator bits explicitly so fixtures do not depend on the STL's
    // uniform_real_distribution implementation.
    auto coordinate{[&] { return static_cast<float>(random() >> 8) / 8388608.0F - 1.0F; }};
    std::vector<StoredRow> rows;
    for (std::size_t i{0}; i < count; ++i) {
        std::vector<float> values(dimension);
        for (auto& value : values) value = coordinate();
        rows.push_back({static_cast<RowId>(i + 1), Row{{std::move(values)}}});
    }
    hnsw_test::writeFixture(temporary.path, "vectors", schema, rows);
    std::vector<std::vector<float>> queries(queryCount, std::vector<float>(dimension));
    for (auto& query : queries) for (auto& value : query) value = coordinate();
    for (auto metric : {DistanceMetric::EUCLIDEAN, DistanceMetric::COSINE}) {
        db.createVectorIndex({metric == DistanceMetric::EUCLIDEAN ? "euclidean" : "cosine",
                              "vectors", "v", metric});
        std::size_t hits{};
        for (const auto& reference : queries) {
            NearestNeighborQuery query{"vectors", "v", reference, metric, k};
            const auto approximate{db.nearestNeighbors(query)};
            assert(approximate.method == NearestNeighborMethod::HNSW);
            assert(approximate.result.rows.size() == k);
            // Independent exhaustive sort, rather than the bounded-heap path.
            std::vector<std::pair<double, RowId>> exact;
            exact.reserve(count);
            for (const auto& row : rows) {
                exact.emplace_back(distance(reference, std::get<std::vector<float>>(row.row.cell(0)), metric), row.id);
            }
            std::partial_sort(exact.begin(), exact.begin() + k, exact.end());
            std::unordered_set<RowId> expected;
            for (std::size_t i{0}; i < k; ++i) expected.insert(exact[i].second);
            for (auto id : approximate.result.rowIds) hits += expected.contains(id);
            // Check the production exact route against the same oracle once.
            if (&reference == &queries.front()) {
                query.mode = NearestNeighborMode::EXACT;
                const auto scanned{db.nearestNeighbors(query)};
                for (std::size_t i{0}; i < k; ++i) assert(scanned.result.rowIds[i] == exact[i].second);
            }
        }
        const auto recall{static_cast<double>(hits) / (queryCount * k)};
        std::cout << (metric == DistanceMetric::EUCLIDEAN ? "euclidean" : "cosine")
                  << " mean recall@10=" << recall << '\n';
        assert(recall >= 0.95);
    }
}
