#include "storage/csv_format.hpp"
#include "db/errors.hpp"

#include <cassert>
#include <sstream>

namespace {

template <typename Action>
void expectStorageError(Action action) {
    bool rejected{false};
    try {
        action();
    } catch (const vrdb::StorageError&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main() {
    using namespace vrdb;
    const CsvFormat format;
    const Schema schema{{Column{"id", DataType::INTEGER}, Column{"text", DataType::TEXT},
                         Column{"embedding", DataType::VECTOR, 3}}};
    const Row row{{std::int64_t{-7}, std::string{"a, \"quoted\" value\nwith a newline"},
                   std::vector<float>{-1.25f, 0, 1e30f}}};
    const std::vector<StoredRow> rows{{41, row}, {99, row}};

    // Format round trips use streams only; opening files belongs to the engine.
    std::stringstream table;
    format.writeTable(table, schema, rows);
    const auto decoded{format.readTable(table, "docs", schema)};
    assert(!decoded.legacy && decoded.rows.size() == rows.size());
    for (std::size_t index{0}; index < rows.size(); ++index) {
        assert(decoded.rows[index].id == rows[index].id);
        assert(decoded.rows[index].row.cells() == rows[index].row.cells());
    }

    std::stringstream catalog;
    format.writeSchema(catalog, "docs", schema);
    const auto restored{format.readSchema(catalog, "docs")};
    assert(restored.size() == schema.size());
    for (std::size_t index{0}; index < schema.size(); ++index) {
        assert(restored.column(index).name == schema.column(index).name);
        assert(restored.column(index).type == schema.column(index).type);
        assert(restored.column(index).vectorDimension == schema.column(index).vectorDimension);
    }

    std::stringstream legacy{"\"id\",\"text\",\"embedding\"\n\"7\",\"old\",\"[1,2,3]\"\n"};
    const auto old{format.readTable(legacy, "docs", schema)};
    assert(old.legacy && old.rows.size() == 1 && old.rows[0].id == 1);

    std::stringstream duplicates;
    format.writeTable(duplicates, schema, {{41, row}, {41, row}});
    expectStorageError([&] { format.readTable(duplicates, "docs", schema); });
    std::stringstream wrongTable{catalog.str()};
    expectStorageError([&] { format.readSchema(wrongTable, "other"); });
    std::stringstream wrongOrder{
        "\"table_name\",\"column_index\",\"column_name\",\"data_type\",\"vector_dimension\"\n"
        "\"docs\",\"1\",\"id\",\"INTEGER\",\"\"\n"};
    expectStorageError([&] { format.readSchema(wrongOrder, "docs"); });

    std::stringstream counter;
    format.writeNextId(counter, 100);
    assert(format.readNextId(counter) == 100);
    for (const std::string text : {"0", "-1", "2x", "18446744073709551616"}) {
        std::stringstream invalid{text};
        expectStorageError([&] { format.readNextId(invalid); });
    }
}
