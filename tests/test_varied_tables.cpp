#include "db/database_manager.hpp"
#include "db/errors.hpp"

#include <cassert>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TemporaryDatabase {
    std::filesystem::path path;

    TemporaryDatabase() {
        std::random_device random;
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = std::filesystem::temp_directory_path() /
                ("vrdb_varied_tables_" + std::to_string(random()));
            if (std::filesystem::create_directory(candidate)) {
                path = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error{"could not create temporary database"};
    }

    ~TemporaryDatabase() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void expectNames(vrdb::DatabaseManager& db, const vrdb::Query& query,
                 const std::vector<std::string>& expected) {
    const auto result = db.select(query);
    assert(result.schema.size() == 1);
    assert(result.schema.column(std::size_t{0}).type == vrdb::DataType::TEXT);
    assert(result.rows.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(std::get<std::string>(result.rows[i].cell(0)) == expected[i]);
    }
}

} // namespace

int main() {
    using namespace vrdb;
    TemporaryDatabase temporary;

    // No id column: vectors come first and in the middle, with different dimensions.
    const Schema inventory{{
        Column{"location", DataType::VECTOR, 2},
        Column{"sku", DataType::TEXT},
        Column{"stock", DataType::INTEGER},
        Column{"category", DataType::TEXT},
        Column{"features", DataType::VECTOR, 3},
        Column{"price_cents", DataType::INTEGER},
        Column{"notes", DataType::TEXT},
    }};
    const std::vector<Row> items{
        Row{{std::vector<float>{0, 0}, std::string{"A"}, int64_t{0},
             std::string{"tools"}, std::vector<float>{1, 0, 0}, int64_t{1250},
             std::string{"Spare, \"small\"\nsecond line"}}},
        Row{{std::vector<float>{3, 4}, std::string{"B"}, int64_t{12},
             std::string{"tools"}, std::vector<float>{0, 1, 0}, int64_t{2500},
             std::string{}}},
        Row{{std::vector<float>{6, 8}, std::string{"C"}, int64_t{-2},
             std::string{"garden"}, std::vector<float>{1, 0, 0}, int64_t{750},
             std::string{"Backordered!"}}},
        Row{{std::vector<float>{0, 5}, std::string{"D"}, int64_t{7},
             std::string{"tools"}, std::vector<float>{1, 0, 0}, int64_t{1800},
             std::string{"Ready to ship"}}},
    };

    {
        DatabaseManager db{temporary.path};
        db.createTable("inventory", inventory);
        for (const auto& item : items) {
            db.insert("inventory", item);
        }
        // Tables also work without vectors, or without any numeric columns.
        db.createTable("sales", Schema{{Column{"region", DataType::TEXT},
            Column{"revenue", DataType::INTEGER}, Column{"returns", DataType::INTEGER}}});
        db.insert("sales", Row{{std::string{"north"}, int64_t{100}, int64_t{2}}});
        db.insert("sales", Row{{std::string{"south"}, int64_t{200}, int64_t{0}}});
        db.insert("sales", Row{{std::string{"west"}, int64_t{-50}, int64_t{1}}});
        db.createTable("labels", Schema{{Column{"label", DataType::TEXT}}});
        for (const auto& label : {"", "a", "aa", "b"}) {
            db.insert("labels", Row{{std::string{label}}});
        }
    }

    // Every query runs after reopening, exercising catalog and CSV persistence.
    DatabaseManager db{temporary.path};
    Query query{};
    query.table = "inventory";
    const auto all = db.select(query);
    assert(all.rows.size() == items.size());
    assert(all.schema.size() == inventory.size());
    for (std::size_t i = 0; i < inventory.size(); ++i) {
        assert(all.schema.column(i).name == inventory.column(i).name);
        assert(all.schema.column(i).type == inventory.column(i).type);
        assert(all.schema.column(i).vectorDimension == inventory.column(i).vectorDimension);
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        assert(all.rows[i].cells() == items[i].cells());
    }

    const std::vector<ComparisonOperator> operators{
        ComparisonOperator::EQUAL, ComparisonOperator::NOT_EQUAL,
        ComparisonOperator::LESS_THAN, ComparisonOperator::LESS_THAN_OR_EQUAL,
        ComparisonOperator::GREATER_THAN, ComparisonOperator::GREATER_THAN_OR_EQUAL,
    };
    const std::vector<std::vector<std::string>> textMatches{
        {"B"}, {"A", "C", "D"}, {"A"}, {"A", "B"}, {"C", "D"}, {"B", "C", "D"},
    };
    const std::vector<std::vector<std::string>> distanceMatches{
        {"B", "D"}, {"A", "C"}, {"A"}, {"A", "B", "D"}, {"C"}, {"B", "C", "D"},
    };
    query.projection = {"sku"};
    for (std::size_t i = 0; i < operators.size(); ++i) {
        query.predicates = {textComparison("sku", operators[i], "B")};
        expectNames(db, query, textMatches[i]);
        query.predicates = {vectorDistance("location", DistanceMetric::EUCLIDEAN,
            {0, 0}, operators[i], 5.0)};
        expectNames(db, query, distanceMatches[i]);
    }

    query.predicates = {
        textComparison("category", ComparisonOperator::EQUAL, "tools"),
        integerComparison("stock", ComparisonOperator::GREATER_THAN, 0),
        integerComparison("price_cents", ComparisonOperator::LESS_THAN, 2000),
        vectorDistance("location", DistanceMetric::EUCLIDEAN, {0, 0},
            ComparisonOperator::LESS_THAN_OR_EQUAL, 5.0),
        vectorDistance("features", DistanceMetric::COSINE, {1, 0, 0},
            ComparisonOperator::LESS_THAN, 0.1),
    };
    expectNames(db, query, {"D"});
    query.projection = {"notes", "features", "price_cents", "location", "sku"};
    const auto projected = db.select(query);
    assert(projected.rows.size() == 1);
    const std::vector<std::size_t> sourceColumns{6, 4, 5, 0, 1};
    for (std::size_t i = 0; i < sourceColumns.size(); ++i) {
        assert(projected.schema.column(i).name == query.projection[i]);
        assert(projected.schema.column(i).type == inventory.column(sourceColumns[i]).type);
        assert(projected.schema.column(i).vectorDimension == inventory.column(sourceColumns[i]).vectorDimension);
        assert(projected.rows[0].cell(i) == items[3].cell(sourceColumns[i]));
    }

    query.projection = {"sku"};
    query.predicates = {textComparison("category", ComparisonOperator::EQUAL, "tools")};
    query.offset = 1;
    query.limit = 1;
    expectNames(db, query, {"B"});
    query.offset = 3;
    expectNames(db, query, {});

    query = Query{};
    query.table = "sales";
    query.projection = {"region"};
    query.predicates = {integerComparison("revenue", ComparisonOperator::GREATER_THAN, 0),
        integerComparison("returns", ComparisonOperator::EQUAL, 0)};
    expectNames(db, query, {"south"});

    query = Query{};
    query.table = "labels";
    query.projection = {"label"};
    query.predicates = {textComparison("label", ComparisonOperator::GREATER_THAN, "a"),
        textComparison("label", ComparisonOperator::LESS_THAN, "b")};
    expectNames(db, query, {"aa"});
    query.predicates = {textComparison("label", ComparisonOperator::EQUAL, "")};
    expectNames(db, query, {""});

    // A vector reference must match the chosen column's dimension.
    query = Query{};
    query.table = "inventory";
    query.predicates = {vectorDistanceLessThan("features", {0, 0}, 1)};
    bool rejected = false;
    try {
        static_cast<void>(db.select(query));
    } catch (const QueryError&) {
        rejected = true;
    }
    assert(rejected);
}
