#pragma once

#include "query/query.hpp"

namespace vrdb {

enum class NearestNeighborMode { AUTO, EXACT };
enum class NearestNeighborMethod { EXACT, HNSW };

struct NearestNeighborQuery {
    std::string table;
    std::string column;
    std::vector<float> referenceVector;
    DistanceMetric metric{DistanceMetric::EUCLIDEAN};
    std::size_t k{10};
    std::vector<std::string> projection;
    std::vector<Predicate> predicates;
    NearestNeighborMode mode{NearestNeighborMode::AUTO};
};

struct NearestNeighborResult {
    QueryResult result;
    std::vector<double> distances;
    NearestNeighborMethod method{NearestNeighborMethod::EXACT};
};

} // namespace vrdb
