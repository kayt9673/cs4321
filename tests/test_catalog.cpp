#include "db/catalog.hpp"
#include "db/errors.hpp"
#include "storage/file_storage_engine.hpp"

#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    using namespace vrdb;
    const auto root{std::filesystem::current_path() / "catalog_test_database"};
    std::filesystem::remove_all(root);

    const Schema schema{{Column{"text", DataType::TEXT}}};
    FileStorageEngine storage{root};
    Catalog catalog;
    catalog.load(storage);
    assert(catalog.listTables().empty());

    catalog.createTable("second", schema, storage);
    catalog.createTable("first", schema, storage);
    Catalog reopened;
    reopened.load(storage);
    assert((reopened.listTables() == std::vector<std::string>{"first", "second"}));
    assert(reopened.getSchema("first").column(0).name == "text");
    reopened.dropTable("first", storage);
    assert(!reopened.hasTable("first"));
    assert(!std::filesystem::exists(root / "catalogs" / "first.csv"));

    // A directory at the target file path forces the schema write to fail.
    const auto failedCatalog{root / "catalogs" / "failed.csv"};
    std::filesystem::create_directory(failedCatalog);
    bool rejected{false};
    try {
        reopened.createTable("failed", schema, storage);
    } catch (const StorageError&) {
        rejected = true;
    }
    assert(rejected && !reopened.hasTable("failed"));
    std::filesystem::remove(failedCatalog);
    reopened.createTable("failed", schema, storage);
    assert(reopened.hasTable("failed"));

    std::filesystem::remove_all(root);
}
