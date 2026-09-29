#include "db/catalog.hpp"
#include "db/errors.hpp"
#include "storage/storage_engine.hpp"

#include <cassert>
#include <stdexcept>
#include <unordered_map>

namespace {

// A catalog should work without files, paths, or CSV encoding.
class MetadataStorage : public vrdb::StorageEngine {
public:
    std::unordered_map<std::string, vrdb::Schema> schemas;
    bool failWrite{false};

    std::vector<std::string> listTables() const override {
        std::vector<std::string> names;
        for (const auto& entry : schemas) names.push_back(entry.first);
        return names;
    }
    vrdb::Schema readSchema(const std::string& name) const override { return schemas.at(name); }
    void writeSchema(const std::string& name, const vrdb::Schema& schema) override {
        if (failWrite) throw vrdb::StorageError{"schema write failed"};
        schemas.emplace(name, schema);
    }
    void removeSchema(const std::string& name) override { schemas.erase(name); }

    void createTable(const std::string&, const vrdb::Schema&) override {
        throw std::logic_error{"catalog must not create row storage"};
    }
    vrdb::RowId appendRow(const std::string&, const vrdb::Schema&, const vrdb::Row&) override {
        throw std::logic_error{"catalog must not append rows"};
    }
    bool updateRow(const std::string&, const vrdb::Schema&, vrdb::RowId, const vrdb::Row&) override {
        throw std::logic_error{"catalog must not update rows"};
    }
    bool deleteRow(const std::string&, const vrdb::Schema&, vrdb::RowId) override {
        throw std::logic_error{"catalog must not delete rows"};
    }
    std::vector<vrdb::StoredRow> readRows(const std::string&, const vrdb::Schema&) const override {
        throw std::logic_error{"catalog must not read rows"};
    }
    void dropTable(const std::string&) override {
        throw std::logic_error{"catalog must not drop row storage"};
    }
};

} // namespace

int main() {
    using namespace vrdb;
    const Schema schema{{Column{"text", DataType::TEXT}}};
    MetadataStorage storage;
    Catalog catalog;
    catalog.load(storage);
    assert(catalog.listTables().empty());

    catalog.createTable("second", schema, storage);
    catalog.createTable("first", schema, storage);
    assert(storage.schemas.size() == 2);
    Catalog reopened;
    reopened.load(storage);
    assert((reopened.listTables() == std::vector<std::string>{"first", "second"}));
    assert(reopened.getSchema("first").column(0).name == "text");
    reopened.dropTable("first", storage);
    assert(!reopened.hasTable("first"));
    assert(storage.schemas.count("first") == 0);

    storage.failWrite = true;
    bool rejected{false};
    try {
        reopened.createTable("failed", schema, storage);
    } catch (const StorageError&) {
        rejected = true;
    }
    assert(rejected && !reopened.hasTable("failed"));
    storage.failWrite = false;
    reopened.createTable("failed", schema, storage);
    assert(reopened.hasTable("failed"));
}
