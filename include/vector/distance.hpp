#pragma once

#include <vector>
#include <span>
#include <initializer_list>

namespace vrdb {

enum class DistanceMetric {
    EUCLIDEAN,
    COSINE
};

// Compute cosine distance, rejecting mismatched dimensions and zero vectors.
double cosineDistance(const std::vector<float>& a, const std::vector<float>& b);
// Compute Euclidean distance between vectors of equal dimension.
double euclideanDistance(const std::vector<float>& a, const std::vector<float>& b);
// Compute vector distance using the requested metric.
double distance(const std::vector<float>& a, const std::vector<float>& b, DistanceMetric metric);

double cosineDistance(std::span<const float> a, std::span<const float> b);
double euclideanDistance(std::span<const float> a, std::span<const float> b);
double distance(std::span<const float> a, std::span<const float> b, DistanceMetric metric);

// Keep brace-initialized calls (including empty vectors) unambiguous after
// introducing the span overloads.
inline double cosineDistance(std::initializer_list<float> a, std::initializer_list<float> b) {
    return cosineDistance(std::span<const float>{a.begin(), a.size()},
                          std::span<const float>{b.begin(), b.size()});
}
inline double euclideanDistance(std::initializer_list<float> a, std::initializer_list<float> b) {
    return euclideanDistance(std::span<const float>{a.begin(), a.size()},
                             std::span<const float>{b.begin(), b.size()});
}
inline double distance(std::initializer_list<float> a, std::initializer_list<float> b, DistanceMetric metric) {
    return distance(std::span<const float>{a.begin(), a.size()},
                    std::span<const float>{b.begin(), b.size()}, metric);
}

} // namespace vrdb
