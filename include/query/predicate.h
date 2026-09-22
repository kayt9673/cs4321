#pragma once

#include "vector/distance.h"

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
    std::vector<float> referenceVector;
    ComparisonOperator op;
    float threshold;
};

using Predicate = std::variant<IntegerPredicate, TextPredicate, VectorPredicate>;

Predicate integerComparison(std::string column, ComparisonOperator op, int64_t value);
Predicate textComparison(std::string column, ComparisonOperator op, std::string value);
Predicate vectorDistance(std::string column,
                         DistanceMetric metric,
                         std::vector<float> referenceVector,
                         ComparisonOperator op,
                         float threshold);
Predicate vectorDistanceLessThan(std::string column, std::vector<float> referenceVector, float threshold);

bool evaluateIntegerComparison(int64_t left, ComparisonOperator op, int64_t right);
bool evaluateTextComparison(const std::string& left, ComparisonOperator op, const std::string& right);
bool evaluateFloatComparison(float left, ComparisonOperator op, float right);

} // namespace vrdb
