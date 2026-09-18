#include "db/database.h"

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

    Database db(root);
    db.createTable("reviews", schema);
    db.insert("reviews", Row({
        int64_t{1},
        std::string{"text with | and % characters"},
        std::vector<float>{1.0f, 2.0f, 3.0f},
    }));

    const auto rows = db.rows("reviews");
    assert(rows.size() == 1);
    assert(std::get<int64_t>(rows[0].value(0)) == 1);
    assert(std::get<std::string>(rows[0].value(1)) == "text with | and % characters");
    assert(std::get<std::vector<float>>(rows[0].value(2)).size() == 3);

    bool threw = false;
    try {
        db.insert("reviews", Row({int64_t{2}, std::string{"bad"}, std::vector<float>{1.0f}}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    std::filesystem::remove_all(root);
    return 0;
}
