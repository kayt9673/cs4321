#include "vector/index.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <hnswlib/hnswlib.h>

namespace vrdb {

void validateHnswOptions(const HnswOptions& options) {
    if (options.M < 2 || options.M > 100 || options.efConstruction < options.M ||
        options.efSearch == 0) {
        throw QueryError{"HNSW requires 2 <= M <= 100, efConstruction >= M, and efSearch > 0"};
    }
}

void validateSearchVector(std::span<const float> values, std::size_t dimension,
                          DistanceMetric metric, const std::string& context) {
    if (dimension == 0 || values.size() != dimension) {
        throw QueryError{context + ": vector dimension mismatch"};
    }
    if (metric != DistanceMetric::EUCLIDEAN && metric != DistanceMetric::COSINE) {
        throw QueryError{context + ": unknown distance metric"};
    }
    bool nonzero{false};
    for (float value : values) {
        if (!std::isfinite(value)) throw QueryError{context + ": non-finite vector coordinate"};
        nonzero = nonzero || value != 0.0F;
    }
    if (metric == DistanceMetric::COSINE && !nonzero) {
        throw QueryError{context + ": cosine distance is undefined for zero vectors"};
    }
}

namespace {

// Space lives longer than the graph, which retains its parameter pointer.
class DatabaseSpace final : public hnswlib::SpaceInterface<double> {
public:
    DatabaseSpace(std::size_t dimension, DistanceMetric metric)
        : dimension_{dimension}, metric_{metric} {}
    std::size_t get_data_size() override { return dimension_ * sizeof(float); }
    hnswlib::DISTFUNC<double> get_dist_func() override { return compute; }
    void* get_dist_func_param() override { return this; }

private:
    static double compute(const void* left, const void* right, const void* parameter) {
        const auto& space{*static_cast<const DatabaseSpace*>(parameter)};
        return distance(std::span<const float>{static_cast<const float*>(left), space.dimension_},
                        std::span<const float>{static_cast<const float*>(right), space.dimension_},
                        space.metric_);
    }
    std::size_t dimension_;
    DistanceMetric metric_;
};

} // namespace

struct HnswVectorIndex::Impl {
    std::size_t dimension;
    DistanceMetric metric;
    HnswOptions options;
    DatabaseSpace space;
    std::vector<RowId> labels;
    std::unique_ptr<hnswlib::HierarchicalNSW<double>> graph;

    Impl(std::size_t dim, DistanceMetric distanceMetric, HnswOptions config)
        : dimension{dim}, metric{distanceMetric}, options{config}, space{dim, distanceMetric} {}
};

HnswVectorIndex::HnswVectorIndex(std::size_t dimension, DistanceMetric metric, HnswOptions options) {
    validateHnswOptions(options);
    if (dimension == 0 || dimension > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
        throw QueryError{"invalid HNSW vector dimension"};
    }
    if (metric != DistanceMetric::EUCLIDEAN && metric != DistanceMetric::COSINE) {
        throw QueryError{"unknown HNSW distance metric"};
    }
    impl_ = std::make_unique<Impl>(dimension, metric, options);
}

HnswVectorIndex::~HnswVectorIndex() = default;

void HnswVectorIndex::build(std::span<const VectorEntry> entries) {
    // The library uses unsigned int internal node identifiers.
    if (entries.size() >= std::numeric_limits<hnswlib::tableint>::max()) {
        throw QueryError{"HNSW population exceeds library node capacity"};
    }
    auto next{std::make_unique<Impl>(impl_->dimension, impl_->metric, impl_->options)};
    std::vector<VectorEntry> ordered(entries.begin(), entries.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    for (const auto& entry : ordered) {
        validateSearchVector(entry.values, next->dimension, next->metric,
                             "HNSW RowId " + std::to_string(entry.id));
        if (!next->labels.empty() && next->labels.back() == entry.id) {
            throw QueryError{"duplicate HNSW RowId " + std::to_string(entry.id)};
        }
        next->labels.push_back(entry.id);
    }
    try {
        if (!ordered.empty()) {
            next->graph = std::make_unique<hnswlib::HierarchicalNSW<double>>(
                &next->space, ordered.size(), next->options.M,
                next->options.efConstruction, next->options.seed);
            for (std::size_t label{0}; label < ordered.size(); ++label) {
                next->graph->addPoint(ordered[label].values.data(), label);
            }
        }
    } catch (const std::exception& error) {
        throw QueryError{"HNSW build failed: " + std::string{error.what()}};
    }
    impl_.swap(next);
}

std::vector<VectorCandidate> HnswVectorIndex::search(std::span<const float> query, std::size_t k) {
    validateSearchVector(query, impl_->dimension, impl_->metric, "HNSW query");
    k = std::min(k, impl_->labels.size());
    if (k == 0) return {};
    try {
        impl_->graph->setEf(std::max(impl_->options.efSearch, k));
        auto found{impl_->graph->searchKnn(query.data(), k)};
        std::vector<VectorCandidate> candidates;
        candidates.reserve(found.size());
        while (!found.empty()) {
            const auto [value, label]{found.top()};
            candidates.push_back({impl_->labels.at(label), value});
            found.pop();
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.distance < b.distance || (a.distance == b.distance && a.id < b.id);
        });
        return candidates;
    } catch (const std::exception& error) {
        throw QueryError{"HNSW search failed: " + std::string{error.what()}};
    }
}

} // namespace vrdb
