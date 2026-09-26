#include "db/errors.hpp"
#include "vector/distance.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <vector>

// Test distance calculations, invalid vectors, and float coordinates.
int main() {
    using namespace vrdb;

    assert(std::fabs(euclideanDistance({0.0, 0.0}, {3.0, 4.0}) - 5.0) < 0.0001);
    assert(std::fabs(distance({0.0, 0.0}, {3.0, 4.0}, DistanceMetric::EUCLIDEAN) - 5.0) < 0.0001);
    assert(std::fabs(cosineDistance({1.0, 0.0}, {1.0, 0.0}) - 0.0) < 0.0001);
    assert(std::fabs(cosineDistance({1.0, 0.0}, {0.0, 1.0}) - 1.0) < 0.0001);

    bool threw{false};
    try {
        euclideanDistance({1.0}, {1.0, 2.0});
    } catch (const QueryError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        cosineDistance({0.0, 0.0}, {1.0, 0.0});
    } catch (const QueryError&) {
        threw = true;
    }
    assert(threw);

    // Float coordinates retain their precision; arithmetic uses double accumulators.
    const float adjacent = std::nextafter(1.0f, 2.0f);
    assert(euclideanDistance({1.0f}, {adjacent}) == static_cast<double>(adjacent) - 1.0);
    const float largest = std::numeric_limits<float>::max();
    assert(euclideanDistance({-largest}, {largest}) == 2.0 * largest);
    assert(std::fabs(cosineDistance({largest}, {largest})) < 0.0001);
    return 0;
}
