#include "db/database.hpp"
#include "db/errors.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    using namespace vrdb;

    const auto root = std::filesystem::temp_directory_path() / "vrdb_storage_test";
    std::filesystem::remove_all(root);

    Schema schema({
        Column("id", ColumnType::INTEGER),
        Column("review", ColumnType::TEXT),
        Column("embedding", ColumnType::VECTOR, 3),
    });

    {
        Database db(root);
        assert(std::filesystem::exists(root / "catalog.csv"));
        assert(std::filesystem::is_directory(root / "tables"));
        db.createTable("reviews", schema);
        assert(std::filesystem::exists(root / "tables" / "reviews.csv"));
        db.insert("reviews", Row({
            std::int64_t{1},
            std::string{"text with |, %, \"quotes\", and\na newline"},
            std::vector<double>{1.0, 2.0, 3.0},
        }));
        db.insert("reviews", Row({
            int64_t{-2},
            std::string{},
            std::vector<double>{0.0, -2.5, 4.25},
        }));

        bool threw = false;
        try {
            db.insert("reviews", Row({int64_t{2}, std::string{"bad"}, std::vector<double>{1.0}}));
        } catch (const SchemaError&) {
            threw = true;
        }
        assert(threw);
    }

    Database db(root);
    assert(db.hasTable("reviews"));
    assert(db.rowCount("reviews") == 2);
    assert(db.listTables() == std::vector<std::string>{"reviews"});
    assert(db.getSchema("reviews").column("embedding").vectorDimension == 3);

    Query query;
    query.table = "reviews";
    const auto result = db.select(query);
    const auto& rows = result.rows;
    assert(rows.size() == 2);
    assert(std::get<int64_t>(rows[0].cell(ColumnId{0})) == 1);
    assert(std::get<std::string>(rows[0].cell(ColumnId{1})) == "text with |, %, \"quotes\", and\na newline");
    assert(std::get<std::vector<double>>(rows[0].cell(ColumnId{2})).size() == 3);
    assert(std::get<int64_t>(rows[1].cell(ColumnId{0})) == -2);
    assert(std::get<std::string>(rows[1].cell(ColumnId{1})).empty());

    bool invalidNameThrew = false;
    try {
        db.createTable("../escape", schema);
    } catch (const DatabaseError&) {
        invalidNameThrew = true;
    }
    assert(invalidNameThrew);

    std::filesystem::remove_all(root);
    return 0;
}
