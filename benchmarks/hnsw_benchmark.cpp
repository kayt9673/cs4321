#include "db/database_manager.hpp"
#include "storage/csv_format.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <unordered_set>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;

std::optional<std::size_t> residentBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<std::size_t>(counters.WorkingSetSize);
    }
#elif defined(__linux__)
    std::ifstream status{"/proc/self/status"};
    std::string key;
    while (status >> key) {
        if (key == "VmRSS:") {
            std::size_t kib{};
            if (status >> kib) return kib * 1024;
        }
        std::string ignored;
        std::getline(status, ignored);
    }
#endif
    return std::nullopt;
}

class TemporaryDatabase {
public:
    TemporaryDatabase() {
        std::random_device random;
        for (int attempt{0}; attempt < 100; ++attempt) {
            path = std::filesystem::temp_directory_path() / ("vrdb_hnsw_benchmark_" + std::to_string(random()));
            if (std::filesystem::create_directory(path)) return;
        }
        throw std::runtime_error{"cannot create benchmark database"};
    }
    ~TemporaryDatabase() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

int main(int argc, char** argv) {
    using namespace vrdb;
    try {
        const std::size_t count{argc > 1 ? std::stoull(argv[1]) : 10000};
        const std::size_t dimension{argc > 2 ? std::stoull(argv[2]) : 16};
        const std::size_t repetitions{argc > 3 ? std::stoull(argv[3]) : 100};
        const std::size_t efSearch{argc > 4 ? std::stoull(argv[4]) : 50};
        if (count == 0 || dimension == 0 || repetitions == 0 || efSearch == 0) {
            throw std::runtime_error{"rows, dimension, queries, and efSearch must be positive"};
        }
        TemporaryDatabase temporary;
        DatabaseManager database{temporary.path};
        const Schema schema{{Column{"embedding", DataType::VECTOR, dimension}}};
        database.createTable("vectors", schema);
        std::mt19937 random{4321};
        auto coordinate{[&] { return static_cast<float>(random() >> 8) / 8388608.0F - 1.0F; }};
        CsvFormat format;
        {
            std::ofstream output{temporary.path / "tables" / "vectors.csv", std::ios::trunc};
            format.writeTable(output, schema, {});
            for (std::size_t i{0}; i < count; ++i) {
                std::vector<float> values(dimension);
                for (auto& value : values) value = coordinate();
                format.writeRow(output, StoredRow{static_cast<RowId>(i + 1), Row{{std::move(values)}}});
            }
            output.close();
            if (!output) throw std::runtime_error{"cannot write fixture"};
            std::ofstream counter{temporary.path / "tables" / "vectors.nextid", std::ios::trunc};
            format.writeNextId(counter, static_cast<RowId>(count + 1));
        }
        VectorIndexDefinition definition{"embedding_hnsw", "vectors", "embedding"};
        definition.options.efSearch = efSearch;
        database.createVectorIndex(definition);
        std::vector<std::vector<float>> references(repetitions, std::vector<float>(dimension));
        for (auto& reference : references) for (auto& value : reference) value = coordinate();
        NearestNeighborQuery query{"vectors", "embedding", references.front()};
        const auto memoryBefore{residentBytes()};
        auto start{Clock::now()};
        database.nearestNeighbors(query);
        const double cold{milliseconds(start)};
        const auto memoryAfter{residentBytes()};
        std::vector<double> approximateTimes, exactTimes;
        std::size_t hits{}, total{};
        for (const auto& reference : references) {
            query.referenceVector = reference;
            query.mode = NearestNeighborMode::AUTO;
            start = Clock::now();
            const auto approximate{database.nearestNeighbors(query)};
            approximateTimes.push_back(milliseconds(start));
            query.mode = NearestNeighborMode::EXACT;
            start = Clock::now();
            const auto exact{database.nearestNeighbors(query)};
            exactTimes.push_back(milliseconds(start));
            const std::unordered_set<RowId> expected(exact.result.rowIds.begin(), exact.result.rowIds.end());
            for (auto id : approximate.result.rowIds) hits += expected.contains(id);
            total += expected.size();
        }
        std::sort(approximateTimes.begin(), approximateTimes.end());
        std::sort(exactTimes.begin(), exactTimes.end());
        std::cout << "rows=" << count << " dimension=" << dimension << " queries=" << repetitions
                  << " efSearch=" << efSearch << '\n'
                  << "cold_build_and_first_query_ms=" << cold << '\n'
                  << "warm_hnsw_median_ms=" << approximateTimes[repetitions / 2] << '\n'
                  << "warm_hnsw_p95_ms=" << approximateTimes[(repetitions - 1) * 95 / 100] << '\n'
                  << "exact_scan_median_ms=" << exactTimes[repetitions / 2] << '\n'
                  << "mean_recall_at_10=" << static_cast<double>(hits) / total << '\n';
        if (memoryBefore && memoryAfter) {
            std::cout << "resident_before_bytes=" << *memoryBefore << '\n'
                      << "resident_after_bytes=" << *memoryAfter << '\n'
                      << "resident_delta_bytes=" << static_cast<std::int64_t>(*memoryAfter) -
                                                    static_cast<std::int64_t>(*memoryBefore) << '\n';
        } else {
            std::cout << "resident_memory=unavailable_on_this_platform\n";
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
