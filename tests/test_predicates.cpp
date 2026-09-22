#include "query/executor.h"

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
        Column("id", Int64Type{}),
        Column("rating", Int64Type{}),
        Column("review", TextType{}),
        Column("embedding", VectorType{2}),
    });

    std::vector<Row> rows({
        Row({int64_t{1}, int64_t{8}, std::string{"keep"}, std::vector<float>{1.0f, 0.0f}}),
        Row({int64_t{2}, int64_t{3}, std::string{"skip"}, std::vector<float>{0.0f, 1.0f}}),
    });

    Query query;
    query.table = "reviews";
    query.projection = {"id", "review"};
    query.predicates.push_back(integerComparison("rating", ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));
    query.predicates.push_back(textComparison("review", ComparisonOperator::EQUAL, "keep"));
    query.predicates.push_back(
        vectorDistance("embedding", DistanceMetric::COSINE, {1.0f, 0.0f}, ComparisonOperator::LESS_THAN_OR_EQUAL, 0.0f));

    QueryExecutor executor;
    const auto result = executor.execute(query, schema, rows);
    assert(result.schema.size() == 2);
    assert(result.rows.size() == 1);
    assert(std::get<int64_t>(result.rows[0].value(0)) == 1);
    assert(std::get<std::string>(result.rows[0].value(1)) == "keep");

    Query emptyResultQuery;
    emptyResultQuery.table = "reviews";
    emptyResultQuery.limit = 0;
    assert(executor.execute(emptyResultQuery, schema, rows).rows.empty());

    return 0;
}
