#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>

int main() {
    using namespace vrdb;

    static_assert(!std::is_constructible_v<Cell, double>);
    static_assert(std::is_same_v<Cell, std::variant<std::int64_t, std::string, std::vector<double>>>);

    const Cell integer{std::int64_t{7}};
    const Cell text{std::string{"hello, \"world\"\n"}};
    const Cell vector{std::vector<double>{1.25, -2.5}};
    auto copy = vector;
    std::get<std::vector<double>>(copy)[0] = 99.0;
    assert(std::get<std::vector<double>>(vector)[0] == 1.25);
    assert(std::get<std::int64_t>(integer) == 7);

    static_assert(std::is_same_v<std::vector<double>::value_type, double>);
    // Preserve precision and range that float32 cannot represent.
    const std::vector<double> precise{std::nextafter(1.0, 2.0), 1.2345678901234567,
                              1e100, 1e-100, std::numeric_limits<double>::max()};
    const Column preciseColumn("precise", ColumnType::VECTOR, precise.size());
    const auto encoded = serializeCellForCsv(Cell{precise});
    const auto decoded = deserializeCellFromCsv(encoded, preciseColumn);
    assert(std::get<std::vector<double>>(decoded) == precise);

    // Cells own their payloads independently of source cells and copies.
    Cell source{std::vector<double>{3.0, 4.0}};
    Cell owned(source);
    std::get<std::vector<double>>(source)[0] = 100.0;
    assert(std::get<std::vector<double>>(owned)[0] == 3.0);
    Cell ownedCopy = owned;
    owned = std::string{"replaced"};
    assert(std::get<std::vector<double>>(ownedCopy) == std::vector<double>({3.0, 4.0}));
    Cell moved = std::move(ownedCopy);
    assert(std::get<std::vector<double>>(moved)[1] == 4.0);
    assert(moved == Cell(std::vector<double>{3.0, 4.0}));
    assert(moved != owned);
    ownedCopy = std::int64_t{9};
    assert(std::get<std::int64_t>(ownedCopy) == 9);

    const Schema schema({
        Column("id", ColumnType::INTEGER),
        Column("text", ColumnType::TEXT),
        Column("embedding", ColumnType::VECTOR, 2),
    });
    const Row row({integer, text, vector});
    schema.validateRow(row);
    assert(std::holds_alternative<std::string>(row.cell(1)));
    assert(std::get<std::string>(row.cell(1)) == std::get<std::string>(text));
    assert(std::get_if<std::vector<double>>(&row.cell(0)) == nullptr);
    assert(std::get_if<std::int64_t>(static_cast<const Cell*>(nullptr)) == nullptr);
    assert(*std::get_if<std::int64_t>(&row.cell(0)) == 7);
    bool wrongType = false;
    try {
        static_cast<void>(std::get<std::string>(row.cell(0)));
    } catch (const std::bad_variant_access&) {
        wrongType = true;
    }
    assert(wrongType);

    std::stringstream csv;
    writeCsvRecord(csv, serializeRowForCsv(row));
    std::vector<std::string> fields;
    assert(readCsvRecord(csv, fields));
    assert(deserializeRowFromCsv(fields, schema).cells() == row.cells());

    // Existing CSV encodings decode into the cells unchanged.
    const auto legacy = deserializeRowFromCsv({"7", "legacy text", "[1.25,-2.5]"}, schema);
    assert(std::get<std::int64_t>(legacy.cell(0)) == 7);
    assert(std::get<std::string>(legacy.cell(1)) == "legacy text");
    assert(std::get<std::vector<double>>(legacy.cell(2)) == std::get<std::vector<double>>(vector));
    const auto column = deserializeColumn("embedding", "VECTOR", "2");
    assert(column.type == ColumnType::VECTOR && column.vectorDimension == 2);
    assert(serializeTypeDimension(column) == "2");
    assert(serializeTypeDimension(schema.column(0)).empty());

    for (const auto type : {ColumnType::INTEGER, ColumnType::TEXT}) {
        bool rejected = false;
        try {
            static_cast<void>(Column("bad", type, 3));
        } catch (const SchemaError&) {
            rejected = true;
        }
        assert(rejected);
    }
    // Public metadata modified after construction must still be validated.
    Column invalid("embedding", ColumnType::VECTOR, 2);
    invalid.vectorDimension = 0;
    bool rejected = false;
    try {
        static_cast<void>(Schema({invalid}));
    } catch (const SchemaError&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
