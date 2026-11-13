#include "hnsw_test_support.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

using namespace vrdb;
using hnsw_test::rejects;

namespace {

// Independent exhaustive-sort oracle, without the production distance helper.
double oracleDistance(const std::vector<float>& a, const std::vector<float>& b, DistanceMetric metric) {
    double squared{}, dot{}, normA{}, normB{};
    for (std::size_t i{0}; i < a.size(); ++i) {
        const double delta{static_cast<double>(a[i]) - b[i]};
        squared += delta * delta;
        dot += static_cast<double>(a[i]) * b[i];
        normA += static_cast<double>(a[i]) * a[i];
        normB += static_cast<double>(b[i]) * b[i];
    }
    return metric == DistanceMetric::EUCLIDEAN ? std::sqrt(squared) :
        1.0 - dot / (std::sqrt(normA) * std::sqrt(normB));
}

void exactQueries() {
    hnsw_test::TemporaryDirectory temporary;
    DatabaseManager db{temporary.path};
    const Schema schema{{Column{"n", DataType::INTEGER}, Column{"tag", DataType::TEXT},
                         Column{"v", DataType::VECTOR, 2}}};
    db.createTable("items", schema);
    const std::vector<StoredRow> rows{
        {90, Row{{int64_t{9}, std::string{"keep"}, std::vector<float>{1, 1}}}},
        {7, Row{{int64_t{7}, std::string{"keep"}, std::vector<float>{1, -1}}}},
        {40, Row{{int64_t{4}, std::string{"skip"}, std::vector<float>{3, 2}}}},
        {2, Row{{int64_t{2}, std::string{"keep"}, std::vector<float>{-1, 1}}}},
        {100, Row{{int64_t{10}, std::string{"keep"}, nullCell()}}}};
    hnsw_test::writeFixture(temporary.path, "items", schema, rows);
    NearestNeighborQuery query{"items", "v", {2, 0}};
    query.projection = {"tag", "n"};
    for (const auto metric : {DistanceMetric::EUCLIDEAN, DistanceMetric::COSINE}) {
        query.metric = metric;
        for (bool filtered : {false, true}) {
            query.predicates.clear();
            if (filtered) query.predicates = {
                integerComparison("n", ComparisonOperator::GREATER_THAN, 2),
                textComparison("tag", ComparisonOperator::EQUAL, "keep")};
            std::vector<std::pair<double, RowId>> oracle;
            for (const auto& stored : rows) {
                if (isNullCell(stored.row.cell(2))) continue;
                if (filtered && (std::get<int64_t>(stored.row.cell(0)) <= 2 ||
                                 std::get<std::string>(stored.row.cell(1)) != "keep")) continue;
                oracle.emplace_back(oracleDistance(std::get<std::vector<float>>(stored.row.cell(2)),
                                                    query.referenceVector, metric), stored.id);
            }
            std::sort(oracle.begin(), oracle.end());
            for (std::size_t k : {0u, 1u, 3u, 100u}) {
                query.k = k;
                const auto result{db.nearestNeighbors(query)};
                assert(result.method == NearestNeighborMethod::EXACT);
                assert(result.result.rows.size() == std::min(k, oracle.size()));
                assert(result.result.schema.column(0).name == "tag");
                assert(result.result.schema.column(1).name == "n");
                for (std::size_t i{0}; i < result.distances.size(); ++i) {
                    assert(result.result.rowIds[i] == oracle[i].second);
                    assert(std::abs(result.distances[i] - oracle[i].first) < 1e-12);
                    const auto row{std::find_if(rows.begin(), rows.end(), [&](const auto& r) {
                        return r.id == result.result.rowIds[i];
                    })};
                    assert(result.result.rows[i].cell(0) == row->row.cell(1));
                }
            }
        }
    }
    query.predicates.clear();
    query.k = 0;
    query.projection = {"missing"};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.projection = {"n", "n"};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.projection.clear();
    query.predicates = {integerComparison("v", ComparisonOperator::EQUAL, 1)};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.predicates.clear();
    query.referenceVector = {1};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.referenceVector = {0, 0};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.metric = DistanceMetric::EUCLIDEAN;
    query.referenceVector = {std::numeric_limits<float>::quiet_NaN(), 1};
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.referenceVector = {1, 0};
    db.createVectorIndex({"items_idx", "items", "v"});
    query.k = 10;
    query.predicates = {vectorDistanceLessThan("v", {2, 0}, 2.0),
                        textComparison("tag", ComparisonOperator::EQUAL, "keep")};
    const auto filteredVectors{db.nearestNeighbors(query)};
    assert(filteredVectors.method == NearestNeighborMethod::EXACT);
    assert((filteredVectors.result.rowIds == std::vector<RowId>{7, 90}));
    query.predicates = {isNull("v")};
    assert(db.nearestNeighbors(query).result.rows.empty());
    query.predicates.clear();
    rejects<SchemaError>([&] { db.createVectorIndex({"bad_type", "items", "n"}); });
    query.column = "n";
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.column = "missing";
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    query.column = "v";
    query.mode = static_cast<NearestNeighborMode>(99);
    rejects<QueryError>([&] { db.nearestNeighbors(query); });
    db.createTable("empty", schema);
    query.table = "empty";
    query.mode = NearestNeighborMode::AUTO;
    query.k = 10;
    assert(db.nearestNeighbors(query).result.rows.empty());
    db.createVectorIndex({"empty_idx", "empty", "v"});
    const auto empty{db.nearestNeighbors(query)};
    assert(empty.method == NearestNeighborMethod::HNSW && empty.result.rows.empty());
    db.insert("empty", Row{{int64_t{1}, nullCell(), nullCell()}});
    assert(db.nearestNeighbors(query).result.rows.empty());
}

void lifecycle() {
    static_assert(std::is_nothrow_move_constructible_v<DatabaseManager>);
    static_assert(std::is_nothrow_move_assignable_v<DatabaseManager>);
    hnsw_test::TemporaryDirectory temporary;
    const Schema schema{{Column{"v", DataType::VECTOR, 2}, Column{"other", DataType::VECTOR, 2}}};
    NearestNeighborQuery query{"items", "v", {1, 0}};
    query.k = 1;
    RowId first{}, second{};
    {
        DatabaseManager db{temporary.path};
        db.createTable("items", schema);
        first = db.insert("items", Row{{std::vector<float>{1, 0}, std::vector<float>{1, 0}}});
        second = db.insert("items", Row{{std::vector<float>{2, 0}, nullCell()}});
        db.createVectorIndex({"idx", "items", "v"});
        db.createVectorIndex({"cosine", "items", "v", DistanceMetric::COSINE});
        db.createVectorIndex({"other", "items", "other"});
        rejects<DatabaseError>([&] { db.createVectorIndex({"idx", "items", "other", DistanceMetric::COSINE}); });
        rejects<DatabaseError>([&] { db.createVectorIndex({"duplicate", "items", "v"}); });
        rejects<DatabaseError>([&] { db.createVectorIndex({"../bad", "items", "v"}); });
        rejects<DatabaseError>([&] { db.createVectorIndex({"missing", "absent", "v"}); });
        rejects<DatabaseError>([&] { db.createVectorIndex({"bad_column", "items", "missing"}); });
        VectorIndexDefinition wrongDimension{"wrong", "items", "v"};
        wrongDimension.dimension = 5;
        rejects<SchemaError>([&] { db.createVectorIndex(wrongDimension); });
        assert(db.listVectorIndexes().size() == 3);
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{first});
        auto other{query}; other.column = "other";
        assert(db.nearestNeighbors(other).result.rowIds == std::vector<RowId>{first});
        auto exact{query}; exact.mode = NearestNeighborMode::EXACT;
        assert(db.nearestNeighbors(exact).method == NearestNeighborMethod::EXACT);
        auto filtered{query}; filtered.predicates = {isNotNull("v")};
        assert(db.nearestNeighbors(filtered).method == NearestNeighborMethod::EXACT);
        // Keep normal scan behavior, including its table order.
        Query scan{}; scan.table = "items";
        assert((db.select(scan, {1}).rowIds == std::vector<RowId>{first, second}));
        db.update("items", first, Row{{std::vector<float>{10, 0}, std::vector<float>{4, 0}}});
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        const auto inserted{db.insert("items", Row{{std::vector<float>{1, 0}, nullCell()}})};
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{inserted});
        db.erase("items", inserted);
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        rejects<DatabaseError>([&] { db.update("items", 999, Row{{nullCell(), nullCell()}}); });
        rejects<DatabaseError>([&] { db.erase("items", 999); });
        rejects<SchemaError>([&] { db.insert("items", Row{{int64_t{1}, nullCell()}}); });
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        // A blocked .nextid temporary file forces an actual storage failure.
        const auto blocked{temporary.path / "tables" / "items.nextid.tmp"};
        std::filesystem::create_directory(blocked);
        rejects<StorageError>([&] { db.insert("items", Row{{nullCell(), nullCell()}}); });
        std::filesystem::remove(blocked);
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        // Index state survives both kinds of database move.
        DatabaseManager moved{std::move(db)};
        assert(moved.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        DatabaseManager destination{temporary.path / "replacement"};
        destination = std::move(moved);
        assert(destination.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
    }
    {
        // Incomplete files do not participate in discovery.
        std::ofstream{temporary.path / "indexes" / "incomplete.csv.tmp"} << "broken";
        DatabaseManager reopened{temporary.path};
        assert(reopened.listVectorIndexes().size() == 3);
        assert(reopened.nearestNeighbors(query).result.rowIds == std::vector<RowId>{second});
        reopened.dropVectorIndex("idx");
        assert(reopened.nearestNeighbors(query).method == NearestNeighborMethod::EXACT);
        rejects<DatabaseError>([&] { reopened.dropVectorIndex("idx"); });
        reopened.dropTable("items");
        assert(reopened.listVectorIndexes().empty());
        assert(!std::filesystem::exists(temporary.path / "indexes" / "cosine.csv"));
        reopened.createTable("items", schema);
        assert(reopened.nearestNeighbors(query).result.rows.empty());
    }
    DatabaseManager reopened{temporary.path};
    assert(reopened.listVectorIndexes().empty());
}

void failedBuildsAndDefinitions() {
    hnsw_test::TemporaryDirectory temporary;
    const Schema schema{{Column{"v", DataType::VECTOR, 2}}};
    {
        DatabaseManager db{temporary.path};
        db.createTable("t", schema);
        const auto id{db.insert("t", Row{{std::vector<float>{0, 0}}})};
        db.createVectorIndex({"cos", "t", "v", DistanceMetric::COSINE});
        // Opening loads definitions only; invalid stored cosine vectors do not
        // trigger graph construction during startup.
        { DatabaseManager metadataOnly{temporary.path}; assert(metadataOnly.listVectorIndexes().size() == 1); }
        NearestNeighborQuery query{"t", "v", {1, 0}, DistanceMetric::COSINE};
        rejects<QueryError>([&] { db.nearestNeighbors(query); });
        rejects<QueryError>([&] { db.nearestNeighbors(query); });
        query.mode = NearestNeighborMode::EXACT;
        rejects<QueryError>([&] { db.nearestNeighbors(query); });
        db.update("t", id, Row{{std::vector<float>{1, 0}}});
        query.mode = NearestNeighborMode::AUTO;
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{id});
        for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            db.update("t", id, Row{{std::vector<float>{invalid, 1}}});
            rejects<QueryError>([&] { db.nearestNeighbors(query); });
            query.mode = NearestNeighborMode::EXACT;
            rejects<QueryError>([&] { db.nearestNeighbors(query); });
            query.mode = NearestNeighborMode::AUTO;
        }
        db.update("t", id, Row{{std::vector<float>{1, 0}}});
        assert(db.nearestNeighbors(query).result.rowIds == std::vector<RowId>{id});
        db.dropVectorIndex("cos");
        std::filesystem::create_directory(temporary.path / "indexes" / "blocked.csv.tmp");
        rejects<StorageError>([&] { db.createVectorIndex({"blocked", "t", "v"}); });
        assert(db.listVectorIndexes().empty());
        std::filesystem::remove(temporary.path / "indexes" / "blocked.csv.tmp");
        db.createVectorIndex({"good", "t", "v"});
    }
    const auto metadata{temporary.path / "indexes" / "good.csv"};
    // Discovery rejects two persisted indexes targeting the same column/metric.
    const auto duplicate{temporary.path / "indexes" / "duplicate.csv"};
    std::ofstream{duplicate} << "vrdb_vector_index,1,duplicate,t,v,EUCLIDEAN,2,16,200,50,100\n";
    rejects<StorageError>([&] { DatabaseManager db{temporary.path}; });
    std::filesystem::remove(duplicate);
    // Mismatched schema dimension is rejected during metadata-only startup.
    std::ofstream{metadata, std::ios::trunc} << "vrdb_vector_index,1,good,t,v,EUCLIDEAN,3,16,200,50,100\n";
    rejects<StorageError>([&] { DatabaseManager db{temporary.path}; });
    std::ofstream{metadata, std::ios::trunc} << "vrdb_vector_index,1,wrong_name,t,v,EUCLIDEAN,2,16,200,50,100\n";
    rejects<StorageError>([&] { DatabaseManager db{temporary.path}; });
    std::ofstream{metadata, std::ios::trunc} << "broken\n";
    rejects<StorageError>([&] { DatabaseManager db{temporary.path}; });
}

} // namespace

int main() {
    exactQueries();
    lifecycle();
    failedBuildsAndDefinitions();
}
