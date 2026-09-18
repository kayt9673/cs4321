#include "vector/distance.h"

#include "db/errors.h"
#include "query/predicate.h"

#include <cmath>

namespace vrdb {
namespace {

void validateSameDimension(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw QueryError("vectors must have matching dimensions");
    }
}

} // namespace

float cosineDistance(const std::vector<float>& a, const std::vector<float>& b) {
    validateSameDimension(a, b);

    float dot = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    if (normA == 0.0f || normB == 0.0f) {
        throw QueryError("cosine distance is undefined for zero vectors");
    }

    return 1.0f - dot / (std::sqrt(normA) * std::sqrt(normB));
}

float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    validateSameDimension(a, b);

    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const float delta = a[i] - b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum);
}

float distance(const std::vector<float>& a, const std::vector<float>& b, DistanceMetric metric) {
    switch (metric) {
    case DistanceMetric::EUCLIDEAN:
        return euclideanDistance(a, b);
    case DistanceMetric::COSINE:
        return cosineDistance(a, b);
    }
    throw QueryError("unknown distance metric");
}

} // namespace vrdb
