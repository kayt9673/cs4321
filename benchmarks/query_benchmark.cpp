#include "db/database_manager.hpp"
#include "query/predicate.hpp"
#include "storage/csv_format.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double run(vrdb::DatabaseManager& database, const vrdb::Query& query,
           const vrdb::QueryExecutionOptions& options,
           std::vector<vrdb::RowId>& expectedIds) {
    const auto start{Clock::now()};
    auto result{database.select(query, options)};
    const auto end{Clock::now()};
    if (expectedIds.empty()) expectedIds = result.rowIds;
    else if (result.rowIds != expectedIds) throw std::runtime_error{"benchmark results differ"};
    return std::chrono::duration<double>(end - start).count();
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

} // namespace

int main(int argc, char** argv) {
    using namespace vrdb;
    const std::size_t rowCount{argc > 1 ? std::stoull(argv[1]) : 30000};
    const std::size_t dimension{argc > 2 ? std::stoull(argv[2]) : 256};
    const std::size_t repetitions{argc > 3 ? std::stoull(argv[3]) : 3};
    const std::size_t predicateCount{argc > 4 ? std::stoull(argv[4]) : 8};
    const std::size_t hardware{std::max(1u, std::thread::hardware_concurrency())};
    const std::size_t workers{std::max<std::size_t>(2, hardware - 1)};
    const auto root{std::filesystem::temp_directory_path() / "vrdb_parallel_query_benchmark"};
    std::filesystem::remove_all(root);

    const Schema schema{{
        Column{"id", DataType::INTEGER},
        Column{"embedding", DataType::VECTOR, dimension},
    }};
    {
        DatabaseManager setup{root};
        setup.createTable("vectors", schema);
    }

    CsvFormat format{};
    std::ofstream table{root / "tables" / "vectors.csv", std::ios::trunc};
    format.writeTable(table, schema, {});
    std::vector<float> values(dimension);
    for (std::size_t row{0}; row < rowCount; ++row) {
        for (std::size_t index{0}; index < dimension; ++index) {
            values[index] = static_cast<float>((row + index) % 101) / 100.0F;
        }
        format.writeRow(table, StoredRow{static_cast<RowId>(row + 1),
            Row{{static_cast<std::int64_t>(row), values}}});
    }
    table.close();
    std::ofstream{root / "tables" / "vectors.nextid", std::ios::trunc}
        << (rowCount + 1) << '\n';

    Query query{};
    query.table = "vectors";
    query.projection = {"id"};
    const std::vector<float> reference(dimension, 0.5F);
    for (std::size_t index{0}; index < predicateCount; ++index) {
        query.predicates.push_back(vectorDistance(
            "embedding", DistanceMetric::EUCLIDEAN, reference,
            ComparisonOperator::LESS_THAN, 1000.0));
    }

    DatabaseManager database{root};
    const QueryExecutionOptions sequential{1, 1024, 1};
    const QueryExecutionOptions parallel{workers, 1024, workers * 2};
    std::vector<RowId> expectedIds{};

    // Warm filesystem caches and worker code paths before timed repetitions.
    run(database, query, sequential, expectedIds);
    run(database, query, parallel, expectedIds);

    std::vector<double> sequentialTimes{};
    std::vector<double> parallelTimes{};
    for (std::size_t repetition{0}; repetition < repetitions; ++repetition) {
        sequentialTimes.push_back(run(database, query, sequential, expectedIds));
        parallelTimes.push_back(run(database, query, parallel, expectedIds));
    }

    const double sequentialMedian{median(sequentialTimes)};
    const double parallelMedian{median(parallelTimes)};
    std::cout << "rows=" << rowCount << " dimension=" << dimension
              << " predicates=" << predicateCount << " workers=" << workers << '\n'
              << "sequential_median_seconds=" << sequentialMedian << '\n'
              << "parallel_median_seconds=" << parallelMedian << '\n'
              << "speedup=" << sequentialMedian / parallelMedian << "x\n";

    std::filesystem::remove_all(root);
}
