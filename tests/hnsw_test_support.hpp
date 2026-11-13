#pragma once

#include "db/database_manager.hpp"
#include "db/errors.hpp"
#include "storage/csv_format.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <random>

namespace hnsw_test {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        std::random_device random;
        for (int attempt{0}; attempt < 100; ++attempt) {
            path = std::filesystem::temp_directory_path() / ("vrdb_hnsw_" + std::to_string(random()));
            if (std::filesystem::create_directory(path)) return;
        }
        throw std::runtime_error{"cannot create test directory"};
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

template <typename Error, typename Action>
void rejects(Action action) {
    bool caught{false};
    try { action(); } catch (const Error&) { caught = true; }
    assert(caught);
}

// Bulk fixtures bypass O(n) per-row CSV insert validation. Use only with a
// freshly created table before any index is built.
inline void writeFixture(const std::filesystem::path& root, const std::string& table,
                         const vrdb::Schema& schema, const std::vector<vrdb::StoredRow>& rows) {
    vrdb::CsvFormat format;
    std::ofstream output{root / "tables" / (table + ".csv"), std::ios::trunc};
    format.writeTable(output, schema, rows);
    output.close();
    assert(output);
    vrdb::RowId next{1};
    for (const auto& row : rows) next = std::max(next, row.id + 1);
    std::ofstream counter{root / "tables" / (table + ".nextid"), std::ios::trunc};
    format.writeNextId(counter, next);
}

} // namespace hnsw_test
