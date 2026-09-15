#include "vector/distance.h"

#include <cmath>
#include <stdexcept>

namespace vrdb {
namespace {

void validateSameDimension(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("vectors must have matching dimensions");
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
        throw std::invalid_argument("cosine distance is undefined for zero vectors");
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

} // namespace vrdb
