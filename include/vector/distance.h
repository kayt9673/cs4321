#pragma once

#include <vector>

namespace vrdb {

enum class DistanceMetric {
    EUCLIDEAN,
    COSINE
};

float cosineDistance(const std::vector<float>& a, const std::vector<float>& b);
float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b);
float distance(const std::vector<float>& a, const std::vector<float>& b, DistanceMetric metric);

} // namespace vrdb
