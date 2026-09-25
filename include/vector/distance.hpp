#pragma once

#include <vector>

namespace vrdb {

enum class DistanceMetric {
    EUCLIDEAN,
    COSINE
};

// Compute cosine distance, rejecting mismatched dimensions and zero vectors.
double cosineDistance(const std::vector<double>& a, const std::vector<double>& b);
// Compute Euclidean distance between vectors of equal dimension.
double euclideanDistance(const std::vector<double>& a, const std::vector<double>& b);
// Compute vector distance using the requested metric.
double distance(const std::vector<double>& a, const std::vector<double>& b, DistanceMetric metric);

} // namespace vrdb
