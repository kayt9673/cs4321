#include "hnsw_test_support.hpp"
#include "storage/index_format.hpp"

#include <cmath>
#include <limits>
#include <sstream>

using namespace vrdb;
using hnsw_test::rejects;

int main() {
    const RowId large{std::numeric_limits<RowId>::max() - 1};
    const std::vector<float> a{1, 0}, b{0, 2}, c{3, 4};
    const std::vector<VectorEntry> entries{{large, c}, {17, b}, {3, a}};
    for (auto metric : {DistanceMetric::EUCLIDEAN, DistanceMetric::COSINE}) {
        HnswVectorIndex index{2, metric};
        index.build(entries);
        auto result{index.search(a, 100)};
        assert(result.size() == 3 && result[0].id == 3);
        for (const auto& candidate : result) {
            const auto& values{candidate.id == 3 ? a : candidate.id == 17 ? b : c};
            assert(std::abs(candidate.distance - distance(a, values, metric)) < 1e-12);
        }
        index.build(entries);
        auto rebuilt{index.search(a, 100)};
        for (std::size_t i{0}; i < result.size(); ++i) {
            assert(result[i].id == rebuilt[i].id && result[i].distance == rebuilt[i].distance);
        }
        assert(index.search(a, 0).empty());
        rejects<QueryError>([&] { index.search(std::vector<float>{1}, 1); });
        rejects<QueryError>([&] {
            const std::vector<VectorEntry> duplicate{{1, a}, {1, b}};
            index.build(duplicate);
        });
        // Failed replacement leaves the previously published index usable.
        assert(index.search(a, 1)[0].id == 3);
        index.build({});
        assert(index.search(a, 10).empty());
    }
    rejects<QueryError>([] { HnswVectorIndex index{0, DistanceMetric::EUCLIDEAN}; });
    rejects<QueryError>([] { HnswVectorIndex index{2, static_cast<DistanceMetric>(99)}; });
    for (HnswOptions options : {HnswOptions{1, 200, 50, 100}, {101, 200, 50, 100},
                               {16, 15, 50, 100}, {16, 200, 0, 100}}) {
        rejects<QueryError>([&] { HnswVectorIndex index{2, DistanceMetric::EUCLIDEAN, options}; });
    }
    for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        HnswVectorIndex index{2, DistanceMetric::EUCLIDEAN};
        std::vector<float> values{invalid, 1};
        rejects<QueryError>([&] { index.search(values, 1); });
        rejects<QueryError>([&] { index.build(std::vector<VectorEntry>{{1, values}}); });
    }
    HnswVectorIndex cosine{2, DistanceMetric::COSINE};
    std::vector<float> zero{0, 0};
    rejects<QueryError>([&] { cosine.build(std::vector<VectorEntry>{{1, zero}}); });
    rejects<QueryError>([&] { cosine.search(zero, 1); });

    // Double accumulation retains finite distances even for extreme floats.
    const float largest{std::numeric_limits<float>::max()};
    std::vector<float> positive{largest}, negative{-largest};
    HnswVectorIndex extreme{1, DistanceMetric::EUCLIDEAN};
    extreme.build(std::vector<VectorEntry>{{1, negative}});
    assert(extreme.search(positive, 1)[0].distance == 2.0 * largest);

    VectorIndexDefinition definition{"idx", "table", "embedding,\n\"quoted\"", DistanceMetric::COSINE};
    definition.dimension = 2;
    std::stringstream stream;
    IndexFormat::write(stream, definition);
    const auto decoded{IndexFormat::read(stream)};
    assert(decoded.column == definition.column && decoded.dimension == 2);
    assert(decoded.options.M == 16 && decoded.options.seed == 100 && decoded.metric == definition.metric);
    for (const std::string text : {
        "", "vrdb_vector_index,2,x,t,v,EUCLIDEAN,2,16,200,50,100\n",
        "vrdb_vector_index,1,x,t,v,EUCLIDEAN,2,-1,200,50,100\n",
        "vrdb_vector_index,1,x,t,v,EUCLIDEAN,2,16,200,0,100\n",
        "vrdb_vector_index,1,x,t,v,EUCLIDEAN,0,16,200,50,100\n",
        "vrdb_vector_index,1,x,t,v,BOGUS,2,16,200,50,100\n",
        "vrdb_vector_index,1,x,t,v,EUCLIDEAN,2,16,200,50,100\ntrailing\n"}) {
        rejects<StorageError>([&] { std::stringstream input{text}; IndexFormat::read(input); });
    }
}
