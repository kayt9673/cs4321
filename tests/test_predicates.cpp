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

    Table table("reviews", Schema({
        Column("id", ColumnType::INTEGER),
        Column("rating", ColumnType::INTEGER),
        Column("embedding", ColumnType::VECTOR, 2),
    }));

    table.addRow(Row({int64_t{1}, int64_t{8}, std::vector<float>{0.0f, 0.0f}}));
    table.addRow(Row({int64_t{2}, int64_t{3}, std::vector<float>{10.0f, 10.0f}}));

    Query query;
    query.table = "reviews";
    query.predicates.push_back(
        Predicate::integerComparison("rating", ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));
    query.predicates.push_back(Predicate::vectorDistanceLessThan("embedding", {0.0f, 0.0f}, 1.0f));

    QueryExecutor executor;
    const auto results = executor.execute(query, table);
    assert(results.size() == 1);
    assert(std::get<int64_t>(results[0].value(0)) == 1);

    return 0;
}
