#include "db/database_manager.hpp"
#include "db/errors.hpp"
#include "query/executor.hpp"
#include "storage/csv_format.hpp"
#include "storage/serialization.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
using namespace vrdb;

template <typename Error, typename Action>
void rejects(Action action) {
    bool caught{false};
    try { action(); } catch (const Error&) { caught = true; }
    assert(caught);
}

void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream{path, std::ios::binary};
    stream << text;
    stream.close();
    assert(stream);
}

std::string read(const std::filesystem::path& path) {
    std::ifstream stream{path};
    return {std::istreambuf_iterator<char>{stream}, {}};
}

void valuesAndQueries() {
    assert(std::get<std::int64_t>(Cell{}) == 0);
    assert(!isNullCell(Cell{}));
    assert(isNullCell(nullCell()));
    assert(cellTypeName(nullCell()) == DataType::EMPTY);
    assert(nullCell() == nullCell()); // Structural equality is not SQL comparison.
    const Schema schema{{Column{"i", DataType::INTEGER, 0}, Column{"t", DataType::TEXT},
                         Column{"v", DataType::VECTOR, 2}, Column{"required", DataType::INTEGER, 0, false}}};
    const Row missing{{nullCell(), nullCell(), nullCell(), std::int64_t{1}}};
    schema.validateRow(missing);
    assert(schema.column(0).nullable);
    rejects<SchemaError>([&] { schema.validateCell(3, nullCell()); });
    rejects<SchemaError>([&] { schema.validateCell(0, std::string{"wrong"}); });
    rejects<SchemaError>([&] { schema.validateCell(2, std::vector<float>{1}); });
    rejects<SchemaError>([&] { schema.validateRow(Row{{nullCell()}}); });
    const Row present{{std::int64_t{2}, std::string{"x"}, std::vector<float>{1, 2}, std::int64_t{1}}};
    const std::vector<StoredRow> rows{{1, missing}, {2, present}, {3, missing}};
    Query query{};
    query.table = "test";
    QueryExecutor executor;
    for (const auto op : {ComparisonOperator::EQUAL, ComparisonOperator::NOT_EQUAL,
                         ComparisonOperator::LESS_THAN, ComparisonOperator::LESS_THAN_OR_EQUAL,
                         ComparisonOperator::GREATER_THAN, ComparisonOperator::GREATER_THAN_OR_EQUAL}) {
        for (const auto& predicate : {integerComparison("i", op, 2), textComparison("t", op, "x"),
                                     vectorDistance("v", DistanceMetric::EUCLIDEAN, {1, 2}, op, 1)}) {
            query.predicates = {predicate};
            const auto result{executor.execute(query, schema, rows)};
            for (const auto id : result.rowIds) assert(id == 2);
        }
    }
    for (const auto& name : {"i", "t", "v"}) {
        query.predicates = {isNull(name)};
        assert((executor.execute(query, schema, rows).rowIds == std::vector<RowId>{1, 3}));
        query.predicates = {isNotNull(name)};
        assert((executor.execute(query, schema, rows).rowIds == std::vector<RowId>{2}));
    }
    query.predicates = {isNull("v"), integerComparison("required", ComparisonOperator::EQUAL, 1)};
    query.projection = {"v", "required"};
    query.offset = 1;
    query.limit = 1;
    const auto result{executor.execute(query, schema, rows)};
    assert((result.rowIds == std::vector<RowId>{3}));
    assert(isNullCell(result.rows[0].cell(0)));
    assert(result.schema.column(0).nullable && result.schema.column(0).vectorDimension == 2);
    assert(!result.schema.column(1).nullable);
    query.predicates = {isNull("absent")};
    rejects<QueryError>([&] { executor.execute(query, schema, rows); });
    query.predicates = {NullPredicate{"i", static_cast<NullOperator>(99)}};
    rejects<QueryError>([&] { executor.execute(query, schema, rows); });
}

void formats() {
    const CsvFormat format;
    const Schema schema{{Column{"value", DataType::TEXT}}};
    std::vector<StoredRow> rows;
    for (const auto& cell : {nullCell(), Cell{std::string{}}, Cell{std::string{"NULL"}},
                             Cell{std::string{"\\N"}}, Cell{std::string{"a,\"b\"\nc\r\nd"}}}) {
        rows.push_back({static_cast<RowId>(rows.size() + 1), Row{{cell}}});
    }
    std::stringstream table;
    format.writeTable(table, schema, rows);
    const auto decoded{format.readTable(table, "test", schema)};
    assert(!decoded.legacy && !decoded.needsUpgrade);
    for (std::size_t i{0}; i < rows.size(); ++i) assert(rows[i].row.cells() == decoded.rows[i].row.cells());
    auto exported{table.str()};
    exported.replace(1, std::string{"__vrdb_row_id"}.size(), "row_id");
    std::stringstream exportInput{exported};
    assert(isNullCell(format.readTable(exportInput, "test", schema).rows[0].row.cell(0)));
    for (const auto& header : {std::string{"value"}, std::string{"__vrdb_row_id"}}) {
        std::stringstream old;
        writeCsvRecord(old, header == "value" ? std::vector<std::string>{header} :
                       std::vector<std::string>{header, "value"});
        writeCsvRecord(old, header == "value" ? std::vector<std::string>{"NULL"} :
                       std::vector<std::string>{"5", "NULL"});
        const auto restored{format.readTable(old, "test", schema)};
        assert(restored.needsUpgrade && !isNullCell(restored.rows[0].row.cell(0)));
    }
    for (const auto& bitmap : {"", "00", "2"}) {
        std::stringstream bad;
        writeCsvRecord(bad, {"__vrdb_row_id", "__vrdb_null_bitmap", "value"});
        writeCsvRecord(bad, {"1", bitmap, ""});
        rejects<StorageError>([&] { format.readTable(bad, "test", schema); });
    }
    std::stringstream payload;
    writeCsvRecord(payload, {"__vrdb_row_id", "__vrdb_null_bitmap", "value"});
    writeCsvRecord(payload, {"1", "1", "not empty"});
    rejects<StorageError>([&] { format.readTable(payload, "test", schema); });
    for (const auto& header : {std::vector<std::string>{"value"},
                              std::vector<std::string>{"__vrdb_row_id", "value"},
                              std::vector<std::string>{"__vrdb_row_id", "__vrdb_null_bitmap", "value"}}) {
        std::stringstream empty;
        writeCsvRecord(empty, header);
        assert(format.readTable(empty, "test", schema).rows.empty());
    }
    for (const auto& text : {"", "\"wrong\"\n", "\"row_id\",\"value\"\n"}) {
        std::stringstream bad{text};
        rejects<StorageError>([&] { format.readTable(bad, "test", schema); });
    }
    std::stringstream oldCatalog;
    writeCsvRecord(oldCatalog, {"table_name", "column_index", "column_name", "data_type", "vector_dimension"});
    writeCsvRecord(oldCatalog, {"test", "0", "value", "TEXT", ""});
    const auto oldSchema{format.readSchema(oldCatalog, "test")};
    assert(!oldSchema.column(0).nullable);
    std::stringstream nullTable;
    format.writeTable(nullTable, schema, {{1, Row{{nullCell()}}}});
    rejects<StorageError>([&] { format.readTable(nullTable, "test", oldSchema); });
    for (const auto& flag : {"", "TRUE", "1", "maybe"}) {
        std::stringstream bad;
        writeCsvRecord(bad, {"table_name", "column_index", "column_name", "data_type", "vector_dimension", "nullable"});
        writeCsvRecord(bad, {"test", "0", "value", "TEXT", "", flag});
        rejects<StorageError>([&] { format.readSchema(bad, "test"); });
    }
    std::stringstream catalog;
    format.writeSchema(catalog, "test", oldSchema);
    assert(!format.readSchema(catalog, "test").column(0).nullable);
    // Existing logical metadata names remain readable in all layouts.
    const Schema reserved{{Column{"__vrdb_row_id", DataType::TEXT}, Column{"__vrdb_null_bitmap", DataType::TEXT}}};
    std::stringstream reservedTable;
    format.writeTable(reservedTable, reserved, {{7, Row{{std::string{"a"}, std::string{"b"}}}}});
    assert(format.readTable(reservedTable, "old", reserved).rows[0].id == 7);
}

void persistenceAndRecovery() {
    const auto root{std::filesystem::temp_directory_path() / "vrdb_null_test"};
    std::filesystem::remove_all(root);
    const Schema schema{{Column{"i", DataType::INTEGER}, Column{"t", DataType::TEXT}, Column{"v", DataType::VECTOR, 2}}};
    const Row missing{{nullCell(), nullCell(), nullCell()}};
    const Row present{{std::int64_t{1}, std::string{}, std::vector<float>{1, 2}}};
    {
        DatabaseManager db{root};
        rejects<SchemaError>([&] { db.createTable("reserved", Schema{{Column{"__vrdb_null_bitmap", DataType::TEXT}}}); });
        db.createTable("test", schema);
        for (int i{0}; i < 9; ++i) db.insert("test", i % 2 == 0 ? missing : present);
        Query query{};
        query.table = "test";
        query.predicates = {isNull("v")};
        query.projection = {"v"};
        query.offset = 1;
        query.limit = 2;
        for (const auto workers : {1u, 2u, 4u}) {
            const auto result{db.select(query, QueryExecutionOptions{workers, 2})};
            assert((result.rowIds == std::vector<RowId>{3, 5}));
            assert(isNullCell(result.rows[0].cell(0)));
        }
        db.update("test", 1, present);
        db.update("test", 2, missing);
        db.erase("test", 9);
    }
    {
        DatabaseManager db{root};
        Query query{};
        query.table = "test";
        const auto result{db.select(query)};
        assert(!isNullCell(result.rows[0].cell(0)) && isNullCell(result.rows[1].cell(0)));
        assert(db.insert("test", missing) == 10);
    }
    // Build old physical layouts and interrupted-upgrade states without relying on timing.
    const auto table{root / "tables" / "test.csv"};
    const auto counter{root / "tables" / "test.nextid"};
    const auto catalog{root / "catalogs" / "test.csv"};
    std::stringstream oldCatalog;
    writeCsvRecord(oldCatalog, {"table_name", "column_index", "column_name", "data_type", "vector_dimension"});
    writeCsvRecord(oldCatalog, {"test", "0", "i", "INTEGER", ""});
    writeCsvRecord(oldCatalog, {"test", "1", "t", "TEXT", ""});
    writeCsvRecord(oldCatalog, {"test", "2", "v", "VECTOR", "2"});
    write(catalog, oldCatalog.str());
    {
        DatabaseManager db{root};
        assert(!db.getSchema("test").column(0).nullable);
        Query query{};
        query.table = "test";
        // Bitmap storage does not override an old catalog's NOT NULL constraint.
        rejects<StorageError>([&] { db.select(query); });
    }
    for (const bool physical : {false, true}) {
        const std::string original{physical ? "\"__vrdb_row_id\",\"i\",\"t\",\"v\"\n\"4\",\"1\",\"\",\"[1,2]\"\n" :
                                             "\"i\",\"t\",\"v\"\n\"1\",\"\",\"[1,2]\"\n"};
        write(table, original);
        write(counter, "50\n"); // Reservation survived, data publication did not.
        write(table.string() + ".tmp", "incomplete");
        write(counter.string() + ".tmp", "999\n"); // Unpublished reservation is ignored.
        DatabaseManager db{root};
        rejects<SchemaError>([&] { db.insert("test", missing); });
        assert(read(table) == original && read(counter) == "50\n");
        db.update("test", physical ? 4 : 1, present);
        assert(read(counter) == "50\n");
        assert(read(table).find("__vrdb_null_bitmap") != std::string::npos);
        assert(db.insert("test", present) == 50);
        db.erase("test", 50);
        assert(db.insert("test", present) == 51);
    }
    // Counter publication failure leaves the table untouched.
    const auto original{read(table)};
    std::filesystem::create_directory(counter.string() + ".tmp");
    {
        DatabaseManager db{root};
        rejects<StorageError>([&] { db.insert("test", present); });
        assert(read(table) == original && read(counter) == "52\n");
    }
    std::filesystem::remove(counter.string() + ".tmp");
    // Data publication fails after reservation: retry must skip the consumed ID.
    std::filesystem::create_directory(table.string() + ".tmp");
    write(table, "\"__vrdb_row_id\",\"i\",\"t\",\"v\"\n\"1\",\"1\",\"\",\"[1,2]\"\n");
    const auto oldTable{read(table)};
    {
        DatabaseManager db{root};
        rejects<StorageError>([&] { db.insert("test", present); });
        assert(read(table) == oldTable && read(counter) == "53\n");
    }
    std::filesystem::remove(table.string() + ".tmp");
    {
        DatabaseManager db{root};
        assert(db.insert("test", present) == 53);
    }
#if defined(_WIN32)
    // Deny delete-sharing so ReplaceFileW must fail after the temporary table
    // has been written. The original must remain readable and retryable.
    const auto beforeReplacement{read(table)};
    const HANDLE locked{CreateFileW(table.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    assert(locked != INVALID_HANDLE_VALUE);
    {
        DatabaseManager db{root};
        rejects<StorageError>([&] { db.update("test", 53, present); });
        assert(read(table) == beforeReplacement);
    }
    assert(CloseHandle(locked));
    {
        DatabaseManager db{root};
        db.update("test", 53, present);
    }
#endif
    for (const auto& invalid : {"0\n", "1\n", "broken\n"}) {
        write(counter, invalid);
        DatabaseManager db{root};
        rejects<StorageError>([&] { db.insert("test", present); });
    }
    std::filesystem::remove(counter);
    {
        DatabaseManager db{root};
        rejects<StorageError>([&] { db.insert("test", present); });
    }
    write(counter, "100\n");
    std::filesystem::remove(table);
    {
        DatabaseManager db{root};
        Query query{};
        query.table = "test";
        rejects<StorageError>([&] { db.select(query); });
    }
    std::filesystem::remove_all(root);
}
} // namespace

int main() {
    valuesAndQueries();
    formats();
    persistenceAndRecovery();
}
