#pragma once

#include <vector>

namespace vrdb {

float cosineDistance(const std::vector<float>& a, const std::vector<float>& b);
float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b);

} // namespace vrdb
