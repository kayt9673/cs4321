#pragma once

#include <vector>

namespace vrdb {

enum class DistanceMetric {
    EUCLIDEAN,
    COSINE
};

double cosineDistance(const std::vector<double>& a, const std::vector<double>& b);
double euclideanDistance(const std::vector<double>& a, const std::vector<double>& b);
double distance(const std::vector<double>& a, const std::vector<double>& b, DistanceMetric metric);

} // namespace vrdb
