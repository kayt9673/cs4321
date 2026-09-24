#include "query/predicate.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

Predicate integerComparison(std::string column, ComparisonOperator op, int64_t value) {
    return IntegerPredicate{std::move(column), op, value};
}

Predicate textComparison(std::string column, ComparisonOperator op, std::string value) {
    return TextPredicate{std::move(column), op, std::move(value)};
}

Predicate vectorDistance(std::string column,
                         DistanceMetric metric,
                         std::vector<double> referenceVector,
                         ComparisonOperator op,
                         double threshold) {
    return VectorPredicate{std::move(column), metric, std::move(referenceVector), op, threshold};
}

Predicate vectorDistanceLessThan(std::string column, std::vector<double> referenceVector, double threshold) {
    return vectorDistance(
        std::move(column), DistanceMetric::EUCLIDEAN, std::move(referenceVector), ComparisonOperator::LESS_THAN, threshold);
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
    throw QueryError("unknown comparison operator");
}

bool evaluateTextComparison(const std::string& left, ComparisonOperator op, const std::string& right) {
    switch (op) {
    case ComparisonOperator::EQUAL:
        return left == right;
    case ComparisonOperator::NOT_EQUAL:
        return left != right;
    case ComparisonOperator::LESS_THAN:
    case ComparisonOperator::LESS_THAN_OR_EQUAL:
    case ComparisonOperator::GREATER_THAN:
    case ComparisonOperator::GREATER_THAN_OR_EQUAL:
        throw QueryError("operator is not supported for text predicates");
    }
    throw QueryError("unknown comparison operator");
}

bool evaluateFloatComparison(double left, ComparisonOperator op, double right) {
    switch (op) {
    case ComparisonOperator::LESS_THAN:
        return left < right;
    case ComparisonOperator::LESS_THAN_OR_EQUAL:
        return left <= right;
    case ComparisonOperator::EQUAL:
    case ComparisonOperator::NOT_EQUAL:
    case ComparisonOperator::GREATER_THAN:
    case ComparisonOperator::GREATER_THAN_OR_EQUAL:
        throw QueryError("operator is not supported for vector distance predicates");
    }
    throw QueryError("unknown comparison operator");
}

} // namespace vrdb
