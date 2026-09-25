#include "db/errors.hpp"
#include "vector/distance.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

// Test distance calculations, invalid vectors, and double precision.
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

    // Neither coordinates nor the resulting distance may narrow to float32.
    assert(euclideanDistance({16777216.0}, {16777217.0}) == 1.0);
    assert(euclideanDistance({0.0}, {1.0000000001}) == 1.0000000001);
    return 0;
}
