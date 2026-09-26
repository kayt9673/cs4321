#pragma once

#include "vector/distance.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vrdb {

enum class ComparisonOperator {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL
};

struct IntegerPredicate {
    std::string column;
    ComparisonOperator op;
    int64_t value;
};

struct TextPredicate {
    std::string column;
    ComparisonOperator op;
    std::string value;
};

struct VectorPredicate {
    std::string column;
    DistanceMetric metric;
    std::vector<double> referenceVector;
    ComparisonOperator op;
    double threshold;
};

using Predicate = std::variant<IntegerPredicate, TextPredicate, VectorPredicate>;

// Build an integer comparison predicate for a named column.
Predicate integerComparison(std::string column, ComparisonOperator op, int64_t value);
// Build a text comparison predicate for a named column.
Predicate textComparison(std::string column, ComparisonOperator op, std::string value);
// Build a vector-distance predicate with a reference vector and threshold.
Predicate vectorDistance(std::string column,
                         DistanceMetric metric,
                         std::vector<double> referenceVector,
                         ComparisonOperator op,
                         double threshold);
// Build a Euclidean-distance predicate with a strict upper threshold.
Predicate vectorDistanceLessThan(std::string column, std::vector<double> referenceVector, double threshold);

// Evaluate an integer comparison using the requested operator.
bool evaluateIntegerComparison(int64_t left, ComparisonOperator op, int64_t right);
// Evaluate a lexicographic text comparison using the requested operator.
bool evaluateTextComparison(const std::string& left, ComparisonOperator op, const std::string& right);
// Evaluate a floating-point comparison using the requested operator.
bool evaluateFloatComparison(double left, ComparisonOperator op, double right);

} // namespace vrdb
