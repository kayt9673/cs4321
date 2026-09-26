#include "db/database.hpp"
#include "db/errors.hpp"

#include <cassert>
#include <filesystem>
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
        db.insert("reviews", Row{{
            std::int64_t{1},
            std::string{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"},
            std::vector<double>{1.0, 2.0, 3.0},
        }});
        db.insert("reviews", Row{{
            int64_t{-2},
            std::string{},
            std::vector<double>{0.0, -2.5, 4.25},
        }});
        db.insert("reviews", Row{{
            int64_t{2},
            std::string{"A little review with spaces and punctuation!"},
            std::vector<double>{1.0, 2.0, 3.0},
        }});
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
    assert(std::get<int64_t>(rows[0].cell(std::size_t{0})) == 1);
    assert(std::get<std::string>(rows[0].cell(std::size_t{1})) == "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
    assert(std::get<std::vector<double>>(rows[0].cell(std::size_t{2})).size() == 3);
    assert(std::get<int64_t>(rows[1].cell(std::size_t{0})) == -2);
    assert(std::get<std::string>(rows[1].cell(std::size_t{1})).empty());
    assert(std::get<std::string>(rows[2].cell(std::size_t{1})) == "A little review with spaces and punctuation!");
    assert(std::get<int64_t>(rows[2].cell(std::size_t{0})) == 2);

    bool invalidNameThrew{false};
    try {
        db.createTable("../escape", schema);
    } catch (const DatabaseError&) {
        invalidNameThrew = true;
    }
    assert(invalidNameThrew);

    db.createTable("other", schema);
    db.insert("other", Row{{int64_t{1}, std::string{"kept"}, std::vector<double>{1, 2, 3}}});
    const auto otherCatalog{root / "catalogs" / "other.csv"};
    const auto otherModified{std::filesystem::last_write_time(otherCatalog)};
    db.dropTable("reviews");
    assert(!std::filesystem::exists(root / "catalogs" / "reviews.csv"));
    assert(!std::filesystem::exists(root / "tables" / "reviews.csv"));
    assert(std::filesystem::last_write_time(otherCatalog) == otherModified);
    DatabaseManager reopened{root};
    assert(reopened.listTables() == std::vector<std::string>{"other"});
    assert(reopened.rowCount("other") == 1);

    std::filesystem::remove_all(root);
    return 0;
}
