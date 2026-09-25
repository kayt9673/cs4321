#include "db/database.h"
#include "db/errors.h"
#include "storage/serialization.h"

#include <cassert>
#include <filesystem>
#include <fstream>
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
        const auto firstId = db.insert("reviews", Row({
            int64_t{1},
            std::string{"text with |, %, \"quotes\", and\na newline"},
            std::vector<float>{1.0f, 2.0f, 3.0f},
        }));
        const auto secondId = db.insert("reviews", Row({
            int64_t{-2},
            std::string{},
            std::vector<float>{0.0f, -2.5f, 4.25f},
        }));
        assert(firstId == 1 && secondId == 2);

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
    assert((result.rowIds == std::vector<RowId>{1, 2}));
    assert(std::get<int64_t>(rows[0].value(ColumnId{0})) == 1);
    assert(std::get<std::string>(rows[0].value(ColumnId{1})) == "text with |, %, \"quotes\", and\na newline");
    assert(std::get<std::vector<float>>(rows[0].value(ColumnId{2})).size() == 3);
    assert(std::get<int64_t>(rows[1].value(ColumnId{0})) == -2);
    assert(std::get<std::string>(rows[1].value(ColumnId{1})).empty());

    db.update("reviews", 2, Row({
        int64_t{99}, std::string{"updated"}, std::vector<float>{-1.0f, 0.0f, 1.0f},
    }));
    assert(db.select(query).rowIds == std::vector<RowId>({1, 2}));
    assert(std::get<std::string>(db.select(query).rows[1].value(1)) == "updated");

    bool invalidUpdateThrew = false;
    try {
        db.update("reviews", 1, Row({int64_t{7}, std::string{"bad"}, std::vector<float>{1.0f}}));
    } catch (const SchemaError&) {
        invalidUpdateThrew = true;
    }
    assert(invalidUpdateThrew);

    db.erase("reviews", 2);
    assert(db.rowCount("reviews") == 1);
    bool missingRowThrew = false;
    try {
        db.erase("reviews", 2);
    } catch (const DatabaseError&) {
        missingRowThrew = true;
    }
    assert(missingRowThrew);
    missingRowThrew = false;
    try {
        db.update("reviews", 2, Row({int64_t{2}, std::string{"missing"}, std::vector<float>{0, 0, 0}}));
    } catch (const DatabaseError&) {
        missingRowThrew = true;
    }
    assert(missingRowThrew);

    const auto thirdId = db.insert("reviews", Row({
        int64_t{3}, std::string{"after delete"}, std::vector<float>{0, 0, 0},
    }));
    assert(thirdId == 3);
    {
        Database reopened(root);
        const auto recovered = reopened.select(query);
        assert((recovered.rowIds == std::vector<RowId>{1, 3}));
        assert(std::get<std::string>(recovered.rows[1].value(1)) == "after delete");
        reopened.erase("reviews", 3);
    }
    {
        Database reopened(root);
        assert(reopened.insert("reviews", Row({
            int64_t{4}, std::string{"no reused ID"}, std::vector<float>{0, 0, 0},
        })) == 4);
    }

    db.createTable("legacy", Schema({Column("value", TextType{})}));
    {
        std::ofstream legacy(root / "tables" / "legacy.csv", std::ios::trunc);
        writeCsvRecord(legacy, {"value"});
        writeCsvRecord(legacy, {"first"});
        writeCsvRecord(legacy, {"second"});
    }
    std::filesystem::remove(root / "tables" / "legacy.nextid");
    Query legacyQuery;
    legacyQuery.table = "legacy";
    assert((db.select(legacyQuery).rowIds == std::vector<RowId>{1, 2}));
    db.update("legacy", 2, Row({std::string{"updated legacy"}}));
    db.erase("legacy", 1);
    assert(db.insert("legacy", Row({std::string{"third"}})) == 3);
    assert((Database(root).select(legacyQuery).rowIds == std::vector<RowId>{2, 3}));
    assert(std::get<std::string>(Database(root).select(legacyQuery).rows[0].value(0)) == "updated legacy");

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
