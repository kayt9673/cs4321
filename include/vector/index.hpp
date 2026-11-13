#pragma once

#include "types/row.hpp"
#include "vector/distance.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace vrdb {

struct HnswOptions {
    std::size_t M{16};
    std::size_t efConstruction{200};
    std::size_t efSearch{50};
    std::size_t seed{100};
};

struct VectorIndexDefinition {
    std::string name;
    std::string table;
    std::string column;
    DistanceMetric metric{DistanceMetric::EUCLIDEAN};
    HnswOptions options{};
    // Inferred from the schema at creation; persisted to detect incompatibility.
    std::size_t dimension{0};
};

struct VectorEntry {
    RowId id;
    std::span<const float> values;
};

struct VectorCandidate {
    RowId id;
    double distance;
};

// Validation shared by exact search and index construction, without changing
// the validation rules of existing storage and distance predicates.
void validateSearchVector(std::span<const float> values, std::size_t dimension,
                          DistanceMetric metric, const std::string& context);
void validateHnswOptions(const HnswOptions& options);

class VectorIndex {
public:
    virtual ~VectorIndex() = default;
    // Input views need only remain valid for this call. Implementations own data.
    virtual void build(std::span<const VectorEntry> entries) = 0;
    virtual std::vector<VectorCandidate> search(std::span<const float> query,
                                               std::size_t k) = 0;
};

class HnswVectorIndex final : public VectorIndex {
public:
    HnswVectorIndex(std::size_t dimension, DistanceMetric metric, HnswOptions options = {});
    ~HnswVectorIndex() override;
    HnswVectorIndex(const HnswVectorIndex&) = delete;
    HnswVectorIndex& operator=(const HnswVectorIndex&) = delete;
    void build(std::span<const VectorEntry> entries) override;
    std::vector<VectorCandidate> search(std::span<const float> query, std::size_t k) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vrdb
