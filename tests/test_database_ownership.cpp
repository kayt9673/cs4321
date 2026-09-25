#include "db/database.hpp"

#include <cassert>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    // Create a unique temporary directory for the ownership test.
    TemporaryDirectory() {
        std::random_device random{};
        for (int attempt{0}; attempt < 100; ++attempt) {
            const auto candidate{std::filesystem::temp_directory_path() /
                ("vrdb_ownership_test_" + std::to_string(random()))};
            if (std::filesystem::create_directory(candidate)) {
                path = candidate;
                return;
            }
        }
        throw std::runtime_error{"could not create ownership test directory"};
    }

    // Remove the temporary directory without throwing on cleanup errors.
    ~TemporaryDirectory() {
        std::error_code error{};
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path{};
};

} // namespace

// Test database moves, result ownership, and persistence after destruction.
int main() {
    using namespace vrdb;

    static_assert(!std::is_copy_constructible_v<DatabaseManager>);
    static_assert(!std::is_copy_assignable_v<DatabaseManager>);
    static_assert(std::is_nothrow_move_constructible_v<DatabaseManager>);
    static_assert(std::is_nothrow_move_assignable_v<DatabaseManager>);

    TemporaryDirectory temporary{};
    const auto originalPath{temporary.path / "original"};
    const auto replacedPath{temporary.path / "replaced"};
    const auto reusedPath{temporary.path / "reused"};
    const std::string text{std::string(65536, 'x') + "|, % \"quoted\"\n"};
    const std::vector<double> embedding(4096, 1.25);
    const Schema schema{{
        Column{"id", ColumnType::INTEGER},
        Column{"review", ColumnType::TEXT},
        Column{"embedding", ColumnType::VECTOR, embedding.size()},
    }};
    Query query{};
    query.table = "reviews";

    const QueryResult survivingResult{[&] {
        DatabaseManager original{originalPath};
        original.createTable("reviews", schema);
        original.insert("reviews", Row{{int64_t{1}, text, embedding}});

        DatabaseManager moved{std::move(original)};
        assert(moved.hasTable("reviews"));
        assert(moved.listTables() == std::vector<std::string>{"reviews"});
        assert(moved.rowCount("reviews") == 1);
        assert(moved.getSchema("reviews").column("embedding").vectorDimension == embedding.size());

        DatabaseManager destination{replacedPath};
        destination.createTable("previous", schema);
        destination.insert("previous", Row{{int64_t{99}, text, embedding}});
        destination = std::move(moved);
        assert(!destination.hasTable("previous"));
        assert(destination.hasTable("reviews"));
        destination.insert("reviews", Row{{int64_t{2}, text, embedding}});
        assert(destination.rowCount("reviews") == 2);

        // A moved-from owner can receive a new database and remain independent.
        original = DatabaseManager{reusedPath};
        original.createTable("independent", schema);
        assert(!original.hasTable("reviews"));
        assert(!destination.hasTable("independent"));

        return destination.select(query);
    }()};

    // Results own their schema and cells beyond the database's lifetime.
    assert(survivingResult.schema.hasColumn("embedding"));
    assert(survivingResult.rows.size() == 2);
    for (std::size_t index{0}; index < survivingResult.rows.size(); ++index) {
        const auto& row{survivingResult.rows[index]};
        assert(std::get<int64_t>(row.cell(ColumnId{0})) == static_cast<int64_t>(index + 1));
        assert(std::get<std::string>(row.cell(ColumnId{1})) == text);
        assert(std::get<std::vector<double>>(row.cell(ColumnId{2})) == embedding);
    }

    DatabaseManager reopened{originalPath};
    assert(reopened.rowCount("reviews") == 2);
    const auto persistedResult{reopened.select(query)};
    assert(persistedResult.rows.size() == survivingResult.rows.size());
    for (std::size_t index{0}; index < persistedResult.rows.size(); ++index) {
        assert(persistedResult.rows[index].cells() == survivingResult.rows[index].cells());
    }

    // Replacing an owner releases its state without removing its persisted data.
    DatabaseManager replaced{replacedPath};
    assert(replaced.hasTable("previous"));
    assert(replaced.rowCount("previous") == 1);
    assert(!replaced.hasTable("reviews"));
    return 0;
}
