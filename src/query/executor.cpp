#include "query/executor.h"

#include "vector/distance.h"

#include <stdexcept>
#include <utility>

namespace vrdb {

Predicate Predicate::integerComparison(std::string column, ComparisonOperator op, int64_t value) {
    Predicate predicate{};
    predicate.kind = Kind::INTEGER_COMPARISON;
    predicate.integer = IntegerPredicate{std::move(column), op, value};
    return predicate;
}

Predicate Predicate::vectorDistanceLessThan(std::string column, std::vector<float> referenceVector, float threshold) {
    Predicate predicate{};
    predicate.kind = Kind::VECTOR_DISTANCE;
    predicate.vectorDistance = VectorDistancePredicate{std::move(column), std::move(referenceVector), threshold};
    return predicate;
}

bool evaluateIntegerComparison(int64_t left, ComparisonOperator op, int64_t right) {
    switch (op) {
    case ComparisonOperator::EQUAL:
        return left == right;
    case ComparisonOperator::NOT_EQUAL:
        return left != right;
    case ComparisonOperator::LESS_THAN:
        return left < right;
    case ComparisonOperator::LESS_THAN_OR_EQUAL:
        return left <= right;
    case ComparisonOperator::GREATER_THAN:
        return left > right;
    case ComparisonOperator::GREATER_THAN_OR_EQUAL:
        return left >= right;
    }
    throw std::invalid_argument("unknown comparison operator");
}

std::vector<Row> QueryExecutor::execute(const Query& query, const Table& table) const {
    if (query.table != table.name()) {
        throw std::invalid_argument("query table does not match provided table");
    }

    std::vector<Row> results;
    for (const auto& row : table.rows()) {
        bool include = true;
        for (const auto& predicate : query.predicates) {
            if (!matches(row, table, predicate)) {
                include = false;
                break;
            }
        }
        if (include) {
            results.push_back(row);
        }
    }
    return results;
}

bool QueryExecutor::matches(const Row& row, const Table& table, const Predicate& predicate) const {
    switch (predicate.kind) {
    case Predicate::Kind::INTEGER_COMPARISON: {
        const int index = table.schema().columnIndex(predicate.integer.column);
        if (index < 0 || table.schema().column(static_cast<std::size_t>(index)).type != ColumnType::INTEGER) {
            throw std::invalid_argument("integer predicate references a non-integer column");
        }

        const auto* value = std::get_if<int64_t>(&row.value(static_cast<std::size_t>(index)));
        if (!value) {
            throw std::invalid_argument("row value is not an integer");
        }
        return evaluateIntegerComparison(*value, predicate.integer.op, predicate.integer.value);
    }
    case Predicate::Kind::VECTOR_DISTANCE: {
        const int index = table.schema().columnIndex(predicate.vectorDistance.column);
        if (index < 0 || table.schema().column(static_cast<std::size_t>(index)).type != ColumnType::VECTOR) {
            throw std::invalid_argument("vector predicate references a non-vector column");
        }

        const auto* value = std::get_if<std::vector<float>>(&row.value(static_cast<std::size_t>(index)));
        if (!value) {
            throw std::invalid_argument("row value is not a vector");
        }
        return euclideanDistance(*value, predicate.vectorDistance.referenceVector) < predicate.vectorDistance.threshold;
    }
    }
    throw std::invalid_argument("unknown predicate kind");
}

} // namespace vrdb
