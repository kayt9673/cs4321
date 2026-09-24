#include "query/executor.hpp"
#include "db/errors.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using namespace vrdb;

    assert(evaluateIntegerComparison(7, ComparisonOperator::EQUAL, 7));
    assert(evaluateIntegerComparison(7, ComparisonOperator::NOT_EQUAL, 8));
    assert(evaluateIntegerComparison(7, ComparisonOperator::LESS_THAN, 8));
    assert(evaluateIntegerComparison(7, ComparisonOperator::LESS_THAN_OR_EQUAL, 7));
    assert(evaluateIntegerComparison(7, ComparisonOperator::GREATER_THAN, 6));
    assert(evaluateIntegerComparison(7, ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));

    Schema schema({
        Column("id", ColumnType::INTEGER),
        Column("rating", ColumnType::INTEGER),
        Column("review", ColumnType::TEXT),
        Column("embedding", ColumnType::VECTOR, 2),
    });

    std::vector<Row> rows({
        Row({int64_t{1}, int64_t{8}, std::string{"keep"}, std::vector<double>{1.0, 0.0}}),
        Row({int64_t{2}, int64_t{3}, std::string{"skip"}, std::vector<double>{0.0, 1.0}}),
    });

    Query query;
    query.table = "reviews";
    query.projection = {"id", "review"};
    query.predicates.push_back(integerComparison("rating", ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));
    query.predicates.push_back(textComparison("review", ComparisonOperator::EQUAL, "keep"));
    query.predicates.push_back(
        vectorDistance("embedding", DistanceMetric::COSINE, {1.0, 0.0}, ComparisonOperator::LESS_THAN_OR_EQUAL, 0.0));

    QueryExecutor executor;
    const auto result = executor.execute(query, schema, rows);
    assert(result.schema.size() == 2);
    assert(result.rows.size() == 1);
    assert(std::get<int64_t>(result.rows[0].cell(0)) == 1);
    assert(std::get<std::string>(result.rows[0].cell(1)) == "keep");

    Query emptyResultQuery;
    emptyResultQuery.table = "reviews";
    emptyResultQuery.limit = 0;
    assert(executor.execute(emptyResultQuery, schema, rows).rows.empty());

    Query preciseQuery;
    preciseQuery.table = "reviews";
    preciseQuery.predicates.push_back(vectorDistanceLessThan("embedding", {0.0, 0.0}, 1.0000000002));
    const std::vector<Row> preciseRows{
        Row({int64_t{3}, int64_t{8}, std::string{"near"}, std::vector<double>{1.0000000001, 0.0}}),
        Row({int64_t{4}, int64_t{8}, std::string{"far"}, std::vector<double>{1.0000000003, 0.0}}),
    };
    const auto preciseResult = executor.execute(preciseQuery, schema, preciseRows);
    assert(preciseResult.rows.size() == 1);
    assert(std::get<int64_t>(preciseResult.rows[0].cell(0)) == 3);
    // Queries retain their own predicate payloads when copied.
    auto copiedQuery = query;
    query.predicates.clear();
    assert(executor.execute(copiedQuery, schema, rows).rows.size() == 1);
    copiedQuery.predicates = {textComparison("rating", ComparisonOperator::EQUAL, "8")};
    bool rejected = false;
    try {
        static_cast<void>(executor.execute(copiedQuery, schema, rows));
    } catch (const QueryError&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
