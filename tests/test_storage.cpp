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
        Column("id", ColumnType::INTEGER),
        Column("review", ColumnType::TEXT),
        Column("embedding", ColumnType::VECTOR, 3),
    });

    {
        Database db(root);
        db.createTable("reviews", schema);
        db.insert("reviews", Row({
            int64_t{1},
            std::string{"text with | and % characters"},
            std::vector<float>{1.0f, 2.0f, 3.0f},
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
    assert(db.rowCount("reviews") == 1);

    Query query;
    query.table = "reviews";
    const auto result = db.select(query);
    const auto& rows = result.rows;
    assert(rows.size() == 1);
    assert(std::get<int64_t>(rows[0].value(0)) == 1);
    assert(std::get<std::string>(rows[0].value(1)) == "text with | and % characters");
    assert(std::get<std::vector<float>>(rows[0].value(2)).size() == 3);

    std::filesystem::remove_all(root);
    return 0;
}
