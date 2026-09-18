#include "query/executor.h"

#include "vector/distance.h"

#include <stdexcept>
#include <utility>

namespace vrdb {
namespace {

struct ResolvedPredicate {
    ColumnId column;
    const Predicate* predicate;
};

const std::string& predicateColumnName(const Predicate& predicate) {
    switch (predicate.kind) {
    case Predicate::Kind::INTEGER_COMPARISON:
        return predicate.integer.column;
    case Predicate::Kind::VECTOR_DISTANCE:
        return predicate.vectorDistance.column;
    }
    throw std::invalid_argument("unknown predicate kind");
}

} // namespace

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

    std::vector<ResolvedPredicate> resolvedPredicates;
    resolvedPredicates.reserve(query.predicates.size());
    for (const auto& predicate : query.predicates) {
        const auto& columnName = predicateColumnName(predicate);
        const auto columnId = table.schema().columnId(columnName);
        if (!columnId) {
            throw std::invalid_argument("Predicate references unknown column: " + columnName);
        }

        const auto& column = table.schema().column(*columnId);
        if (predicate.kind == Predicate::Kind::INTEGER_COMPARISON && !isInteger(column.type)) {
            throw std::invalid_argument(
                "Integer predicate references non-integer column: " + columnName);
        }
        if (predicate.kind == Predicate::Kind::VECTOR_DISTANCE) {
            if (!isVector(column.type)) {
                throw std::invalid_argument(
                    "Vector predicate references non-vector column: " + columnName);
            }

            const auto expectedDimension = std::get<VectorType>(column.type).dimension();
            const auto receivedDimension = predicate.vectorDistance.referenceVector.size();
            if (expectedDimension != receivedDimension) {
                throw std::invalid_argument(
                    "Vector predicate for column '" + columnName + "' expects dimension " +
                    std::to_string(expectedDimension) + " but received " +
                    std::to_string(receivedDimension));
            }
        }

        resolvedPredicates.push_back(ResolvedPredicate{*columnId, &predicate});
    }

    std::vector<Row> results;
    for (const auto& row : table.rows()) {
        bool include = true;
        for (const auto& resolved : resolvedPredicates) {
            if (!matches(row, resolved.column, *resolved.predicate)) {
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

bool QueryExecutor::matches(const Row& row, ColumnId column, const Predicate& predicate) const {
    switch (predicate.kind) {
    case Predicate::Kind::INTEGER_COMPARISON: {
        const auto* value = std::get_if<std::int64_t>(&row.value(column));
        if (!value) {
            throw std::invalid_argument("Row value is not an integer");
        }
        return evaluateIntegerComparison(*value, predicate.integer.op, predicate.integer.value);
    }
    case Predicate::Kind::VECTOR_DISTANCE: {
        const auto* value = std::get_if<VectorValue>(&row.value(column));
        if (!value) {
            throw std::invalid_argument("Row value is not a vector");
        }
        return euclideanDistance(*value, predicate.vectorDistance.referenceVector) < predicate.vectorDistance.threshold;
    }
    }
    throw std::invalid_argument("unknown predicate kind");
}

} // namespace vrdb
