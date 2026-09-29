#include "db/database_manager.hpp"
#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Test persistence, permissive TEXT values, and rejection of invalid rows and names.
int main() {
    using namespace vrdb;

    const auto root{std::filesystem::temp_directory_path() / "vrdb_storage_test"};
    std::filesystem::remove_all(root);

    Schema schema{{
        Column{"id", DataType::INTEGER},
        Column{"review", DataType::TEXT},
        Column{"embedding", DataType::VECTOR, 3},
    }};

    {
        DatabaseManager db{root};
        assert(std::filesystem::is_directory(root / "catalogs"));
        assert(!std::filesystem::exists(root / "catalog.csv"));
        assert(std::filesystem::is_directory(root / "tables"));
        db.createTable("reviews", schema);
        assert(std::filesystem::exists(root / "catalogs" / "reviews.csv"));
        assert(std::filesystem::exists(root / "tables" / "reviews.csv"));
        assert(db.insert("reviews", Row{{
            std::int64_t{1},
            std::string{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"},
            std::vector<float>{1.0, 2.0, 3.0},
        }}) == 1);
        assert(db.insert("reviews", Row{{
            int64_t{-2},
            std::string{},
            std::vector<float>{0.0, -2.5, 4.25},
        }}) == 2);
        assert(db.insert("reviews", Row{{
            int64_t{2},
            std::string{"A little review with spaces and punctuation!"},
            std::vector<float>{1.0, 2.0, 3.0},
        }}) == 3);
    }

    DatabaseManager db{root};
    assert(db.hasTable("reviews"));
    assert(db.rowCount("reviews") == 3);
    assert(db.listTables() == std::vector<std::string>{"reviews"});
    assert(db.getSchema("reviews").column("embedding").vectorDimension == 3);

    Query query{};
    query.table = "reviews";
    const auto result{db.select(query)};
    const auto& rows{result.rows};
    assert(rows.size() == 3);
    assert((result.rowIds == std::vector<RowId>{1, 2, 3}));
    assert(std::get<int64_t>(rows[0].cell(std::size_t{0})) == 1);
    assert(std::get<std::string>(rows[0].cell(std::size_t{1})) == "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
    assert(std::get<std::vector<float>>(rows[0].cell(std::size_t{2})).size() == 3);
    assert(std::get<int64_t>(rows[1].cell(std::size_t{0})) == -2);
    assert(std::get<std::string>(rows[1].cell(std::size_t{1})).empty());
    assert(std::get<std::string>(rows[2].cell(std::size_t{1})) == "A little review with spaces and punctuation!");
    assert(std::get<int64_t>(rows[2].cell(std::size_t{0})) == 2);

    db.update("reviews", 2, Row{{int64_t{99}, std::string{"updated"}, std::vector<float>{0, 0, 0}}});
    assert(std::get<std::string>(db.select(query).rows[1].cell(1)) == "updated");
    db.erase("reviews", 2);
    assert(db.insert("reviews", Row{{int64_t{4}, std::string{"new"}, std::vector<float>{0, 0, 0}}}) == 4);
    assert((DatabaseManager{root}.select(query).rowIds == std::vector<RowId>{1, 3, 4}));
    bool missingRowThrew{false};
    try {
        db.erase("reviews", 2);
    } catch (const DatabaseError&) {
        missingRowThrew = true;
    }
    assert(missingRowThrew);
    missingRowThrew = false;
    try {
        db.update("reviews", 2, Row{{int64_t{0}, std::string{}, std::vector<float>{0, 0, 0}}});
    } catch (const DatabaseError&) {
        missingRowThrew = true;
    }
    assert(missingRowThrew);

    bool invalidNameThrew{false};
    try {
        db.createTable("../escape", schema);
    } catch (const DatabaseError&) {
        invalidNameThrew = true;
    }
    assert(invalidNameThrew);

    std::filesystem::create_directory(root / "tables" / "blocked.csv");
    bool blockedCreateThrew{false};
    try {
        db.createTable("blocked", schema);
    } catch (const StorageError&) {
        blockedCreateThrew = true;
    }
    assert(blockedCreateThrew && !db.hasTable("blocked"));

    std::filesystem::create_directory(root / "catalogs" / "catalog_blocked.csv");
    blockedCreateThrew = false;
    try {
        db.createTable("catalog_blocked", schema);
    } catch (const StorageError&) {
        blockedCreateThrew = true;
    }
    assert(blockedCreateThrew && !db.hasTable("catalog_blocked"));
    assert(!std::filesystem::exists(root / "tables" / "catalog_blocked.csv"));

    db.createTable("other", schema);
    db.insert("other", Row{{int64_t{1}, std::string{"kept"}, std::vector<float>{1, 2, 3}}});
    const auto otherCatalog{root / "catalogs" / "other.csv"};
    const auto otherModified{std::filesystem::last_write_time(otherCatalog)};
    db.dropTable("reviews");
    assert(!std::filesystem::exists(root / "catalogs" / "reviews.csv"));
    assert(!std::filesystem::exists(root / "tables" / "reviews.csv"));
    assert(!std::filesystem::exists(root / "tables" / "reviews.nextid"));
    assert(std::filesystem::last_write_time(otherCatalog) == otherModified);
    DatabaseManager reopened{root};
    assert(reopened.listTables() == std::vector<std::string>{"other"});
    assert(reopened.rowCount("other") == 1);

    std::filesystem::remove_all(root);

    const auto legacyRoot{std::filesystem::temp_directory_path() / "vrdb_legacy_rows_test"};
    std::filesystem::remove_all(legacyRoot);
    {
        DatabaseManager setup{legacyRoot};
        setup.createTable("legacy", schema);
    }
    {
        std::ofstream table{legacyRoot / "tables" / "legacy.csv", std::ios::trunc};
        writeCsvRecord(table, {"id", "review", "embedding"});
        writeCsvRecord(table, {"10", "old", "[1,2,3]"});
        writeCsvRecord(table, {"20", "same", "[1,2,3]"});
    }
    std::filesystem::remove(legacyRoot / "tables" / "legacy.nextid");
    {
        DatabaseManager legacy{legacyRoot};
        Query oldQuery{};
        oldQuery.table = "legacy";
        assert((legacy.select(oldQuery).rowIds == std::vector<RowId>{1, 2}));
        legacy.erase("legacy", 1);
        assert(legacy.insert("legacy", Row{{int64_t{20}, std::string{"same"},
                                              std::vector<float>{1, 2, 3}}}) == 3);
        assert((legacy.select(oldQuery).rowIds == std::vector<RowId>{2, 3}));
    }
    DatabaseManager legacyReopened{legacyRoot};
    Query oldQuery{};
    oldQuery.table = "legacy";
    assert((legacyReopened.select(oldQuery).rowIds == std::vector<RowId>{2, 3}));
    std::filesystem::remove_all(legacyRoot);
    return 0;
}
