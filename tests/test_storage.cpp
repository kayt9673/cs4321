#include "db/database.h"
#include "db/errors.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    using namespace vrdb;

    const auto root = std::filesystem::temp_directory_path() / "vrdb_storage_test";
    std::filesystem::remove_all(root);

    Schema schema({
        Column("id", Int64Type{}),
        Column("review", TextType{}),
        Column("embedding", VectorType{3}),
    });

    {
        Database db(root);
        assert(std::filesystem::exists(root / "catalog.csv"));
        assert(std::filesystem::is_directory(root / "tables"));
        db.createTable("reviews", schema);
        assert(std::filesystem::exists(root / "tables" / "reviews.csv"));
        db.insert("reviews", Row({
            int64_t{1},
            std::string{"text with |, %, \"quotes\", and\na newline"},
            std::vector<float>{1.0f, 2.0f, 3.0f},
        }));
        db.insert("reviews", Row({
            int64_t{-2},
            std::string{},
            std::vector<float>{0.0f, -2.5f, 4.25f},
        }));

        bool threw = false;
        try {
            db.insert("reviews", Row({int64_t{2}, std::string{"bad"}, std::vector<float>{1.0f}}));
        } catch (const SchemaError&) {
            threw = true;
        }
        assert(threw);
    }

    Database db(root);
    assert(db.hasTable("reviews"));
    assert(db.rowCount("reviews") == 2);
    assert(db.listTables() == std::vector<std::string>{"reviews"});
    assert(std::get<VectorType>(db.getSchema("reviews").column("embedding").type).dimension() == 3);

    Query query;
    query.table = "reviews";
    const auto result = db.select(query);
    const auto& rows = result.rows;
    assert(rows.size() == 2);
    assert(std::get<int64_t>(rows[0].value(ColumnId{0})) == 1);
    assert(std::get<std::string>(rows[0].value(ColumnId{1})) == "text with |, %, \"quotes\", and\na newline");
    assert(std::get<std::vector<float>>(rows[0].value(ColumnId{2})).size() == 3);
    assert(std::get<int64_t>(rows[1].value(ColumnId{0})) == -2);
    assert(std::get<std::string>(rows[1].value(ColumnId{1})).empty());

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
