#include "db/errors.h"
#include "vector/distance.h"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

int main() {
    using namespace vrdb;

    assert(std::fabs(euclideanDistance({0.0f, 0.0f}, {3.0f, 4.0f}) - 5.0f) < 0.0001f);
    assert(std::fabs(distance({0.0f, 0.0f}, {3.0f, 4.0f}, DistanceMetric::EUCLIDEAN) - 5.0f) < 0.0001f);
    assert(std::fabs(cosineDistance({1.0f, 0.0f}, {1.0f, 0.0f}) - 0.0f) < 0.0001f);
    assert(std::fabs(cosineDistance({1.0f, 0.0f}, {0.0f, 1.0f}) - 1.0f) < 0.0001f);

    bool threw = false;
    try {
        euclideanDistance({1.0f}, {1.0f, 2.0f});
    } catch (const QueryError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        cosineDistance({0.0f, 0.0f}, {1.0f, 0.0f});
    } catch (const QueryError&) {
        threw = true;
    }
    assert(threw);

    return 0;
}
