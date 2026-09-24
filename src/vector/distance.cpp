#include "vector/distance.hpp"

#include "db/errors.hpp"

#include <cmath>

namespace vrdb {
namespace {

void validateSameDimension(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) {
        throw QueryError("vectors must have matching dimensions");
    }
}

} // namespace

double cosineDistance(const std::vector<double>& a, const std::vector<double>& b) {
    validateSameDimension(a, b);

    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    if (normA == 0.0 || normB == 0.0) {
        throw QueryError("cosine distance is undefined for zero vectors");
    }

    return 1.0 - dot / (std::sqrt(normA) * std::sqrt(normB));
}

double euclideanDistance(const std::vector<double>& a, const std::vector<double>& b) {
    validateSameDimension(a, b);

    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double delta = a[i] - b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum);
}

double distance(const std::vector<double>& a, const std::vector<double>& b, DistanceMetric metric) {
    switch (metric) {
    case DistanceMetric::EUCLIDEAN:
        return euclideanDistance(a, b);
    case DistanceMetric::COSINE:
        return cosineDistance(a, b);
    }
    throw QueryError("unknown distance metric");
}

} // namespace vrdb
