#pragma once

#include <cstdint>
#include <string>
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

struct VectorDistancePredicate {
    std::string column;
    std::vector<float> referenceVector;
    float threshold;
};

struct Predicate {
    enum class Kind {
        INTEGER_COMPARISON,
        VECTOR_DISTANCE
    };

    Kind kind;
    IntegerPredicate integer;
    VectorDistancePredicate vectorDistance;

    static Predicate integerComparison(std::string column, ComparisonOperator op, int64_t value);
    static Predicate vectorDistanceLessThan(std::string column, std::vector<float> referenceVector, float threshold);
};

bool evaluateIntegerComparison(int64_t left, ComparisonOperator op, int64_t right);

} // namespace vrdb
