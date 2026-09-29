#include "storage/csv_format.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <charconv>
#include <limits>
#include <unordered_set>
#include <utility>

namespace vrdb {
namespace {

const std::vector<std::string> catalogHeader{
    "table_name", "column_index", "column_name", "data_type", "vector_dimension"
};
constexpr const char* rowIdHeader{"__vrdb_row_id"};

RowId parseRowId(const std::string& text, const std::string& context) {
    RowId id{};
    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), id)};
    if (error != std::errc{} || end != text.data() + text.size() || id == 0) {
        throw StorageError{"invalid RowId in " + context + ": " + text};
    }
    return id;
}

} // namespace

// Check the table header and deserialize all stored rows.
CsvFormat::TableData CsvFormat::readTable(std::istream& input, const std::string& tableName,
                                         const Schema& schema) const {
    std::vector<std::string> fields{};
    if (!readCsvRecord(input, fields)) {
        throw StorageError{"table CSV is missing its header: " + tableName};
    }
    const bool legacy{fields.size() == schema.size()};
    const std::size_t offset{legacy ? 0u : 1u};
    if (fields.size() != schema.size() + offset || (!legacy && fields.front() != rowIdHeader)) {
        throw StorageError{"table CSV header does not match schema: " + tableName};
    }
    for (std::size_t index{0}; index < schema.size(); ++index) {
        if (fields[index + offset] != schema.column(index).name) {
            throw StorageError{"table CSV header does not match schema: " + tableName};
        }
    }

    TableData data{{}, legacy};
    std::unordered_set<RowId> seen{};
    while (readCsvRecord(input, fields)) {
        if (fields.size() != schema.size() + offset) {
            throw StorageError{"stored row width does not match schema: " + tableName};
        }
        RowId id{};
        if (legacy) {
            if (data.rows.size() >= std::numeric_limits<RowId>::max() - 1) {
                throw StorageError{"too many rows in " + tableName};
            }
            id = static_cast<RowId>(data.rows.size()) + 1;
        } else {
            id = parseRowId(fields.front(), tableName);
            fields.erase(fields.begin());
            if (!seen.insert(id).second) {
                throw StorageError{"duplicate RowId in " + tableName};
            }
        }
        data.rows.push_back(StoredRow{id, deserializeRowFromCsv(fields, schema)});
    }
    return data;
}

void CsvFormat::writeTable(std::ostream& output, const Schema& schema,
                            const std::vector<StoredRow>& rows) const {
    std::vector<std::string> header{rowIdHeader};
    for (const auto& column : schema.columns()) {
        header.push_back(column.name);
    }
    writeCsvRecord(output, header);
    for (const auto& row : rows) {
        writeRow(output, row);
    }
}

void CsvFormat::writeRow(std::ostream& output, const StoredRow& row) const {
    auto fields{serializeRowForCsv(row.row)};
    fields.insert(fields.begin(), std::to_string(row.id));
    writeCsvRecord(output, fields);
}

Schema CsvFormat::readSchema(std::istream& input, const std::string& tableName) const {
    // read catalog header
    std::vector<std::string> fields{};
    if (!readCsvRecord(input, fields) || fields != catalogHeader) {
        throw StorageError{"invalid catalog header for " + tableName};
    }
    std::vector<Column> columns{};
    while (readCsvRecord(input, fields)) {
        if (fields.size() != catalogHeader.size() || fields[0] != tableName ||
            fields[1] != std::to_string(columns.size())) {
            throw StorageError{"invalid catalog record for " + tableName};
        }
        columns.push_back(deserializeColumn(fields[2], fields[3], fields[4]));
    }
    return Schema{std::move(columns)};
}

void CsvFormat::writeSchema(std::ostream& output, const std::string& tableName,
                             const Schema& schema) const {
    writeCsvRecord(output, catalogHeader);
    for (std::size_t index{0}; index < schema.size(); ++index) {
        const auto& column{schema.column(index)};
        writeCsvRecord(output, {tableName, std::to_string(index), column.name,
                                std::string{dataTypeName(column.type)}, serializeTypeDimension(column)});
    }
}

RowId CsvFormat::readNextId(std::istream& input) const {
    std::string value{};
    if (!std::getline(input, value)) {
        throw StorageError{"missing RowId counter"};
    }
    return parseRowId(value, "counter");
}

void CsvFormat::writeNextId(std::ostream& output, RowId id) const {
    output << id << '\n';
}

} // namespace vrdb
