#include "storage/csv_format.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <charconv>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace vrdb {
namespace {

const std::vector<std::string> catalogHeader{
    "table_name", "column_index", "column_name", "data_type", "vector_dimension", "nullable"
};
const std::vector<std::string> oldCatalogHeader{catalogHeader.begin(), catalogHeader.end() - 1};
constexpr const char* rowIdHeader{"__vrdb_row_id"};
constexpr const char* nullBitmapHeader{"__vrdb_null_bitmap"};

RowId parseRowId(const std::string& text, const std::string& context) {
    RowId id{};
    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), id)};
    if (error != std::errc{} || end != text.data() + text.size() || id == 0) {
        throw StorageError{"invalid RowId in " + context + ": " + text};
    }
    return id;
}

} // namespace

struct CsvFormat::TableReader::Impl {
    std::istream& input;
    std::string tableName;
    Schema schema;
    bool legacyFormat{false};
    bool bitmapFormat{false};
    bool reachedEnd{false};
    RowId nextLegacyId{1};
    std::unordered_set<RowId> seen;

    Impl(std::istream& stream, std::string name, Schema tableSchema)
        : input{stream}, tableName{std::move(name)}, schema{std::move(tableSchema)} {
        std::vector<std::string> fields{};
        if (!readCsvRecord(input, fields)) {
            throw StorageError{"table CSV is missing its header: " + tableName};
        }
        legacyFormat = fields.size() == schema.size();
        bitmapFormat = fields.size() == schema.size() + 2;
        const std::size_t offset{legacyFormat ? 0u : (bitmapFormat ? 2u : 1u)};
        if (fields.size() != schema.size() + offset ||
            (!legacyFormat && fields.front() != rowIdHeader &&
             !(bitmapFormat && fields.front() == "row_id")) ||
            (bitmapFormat && fields[1] != nullBitmapHeader)) {
            throw StorageError{"table CSV header does not match schema: " + tableName};
        }
        for (std::size_t index{0}; index < schema.size(); ++index) {
            if (fields[index + offset] != schema.column(index).name) {
                throw StorageError{"table CSV header does not match schema: " + tableName};
            }
        }
    }
};

CsvFormat::TableReader::TableReader(std::istream& input, std::string tableName, Schema schema)
    : impl_{std::make_unique<Impl>(input, std::move(tableName), std::move(schema))} {}

CsvFormat::TableReader::~TableReader() = default;
CsvFormat::TableReader::TableReader(TableReader&&) noexcept = default;
CsvFormat::TableReader& CsvFormat::TableReader::operator=(TableReader&&) noexcept = default;

std::vector<StoredRow> CsvFormat::TableReader::readNext(std::size_t maxRows) {
    if (maxRows == 0) {
        throw std::invalid_argument{"CSV batch size must be greater than zero"};
    }
    std::vector<StoredRow> rows{};
    rows.reserve(maxRows);
    std::vector<std::string> fields{};
    const std::size_t offset{impl_->legacyFormat ? 0u : (impl_->bitmapFormat ? 2u : 1u)};
    while (rows.size() < maxRows) {
        if (!readCsvRecord(impl_->input, fields)) {
            impl_->reachedEnd = true;
            break;
        }
        if (fields.size() != impl_->schema.size() + offset) {
            throw StorageError{"stored row width does not match schema: " + impl_->tableName};
        }
        RowId id{};
        if (impl_->legacyFormat) {
            if (impl_->nextLegacyId == std::numeric_limits<RowId>::max()) {
                throw StorageError{"too many rows in " + impl_->tableName};
            }
            id = impl_->nextLegacyId++;
        } else {
            id = parseRowId(fields.front(), impl_->tableName);
            fields.erase(fields.begin());
            if (!impl_->seen.insert(id).second) {
                throw StorageError{"duplicate RowId in " + impl_->tableName};
            }
        }
        std::string bitmap;
        if (impl_->bitmapFormat) {
            bitmap = std::move(fields.front());
            fields.erase(fields.begin());
            if (bitmap.size() != impl_->schema.size()) throw StorageError{"invalid NULL bitmap width"};
        }
        rows.push_back(StoredRow{id, deserializeRowFromCsv(fields, impl_->schema, bitmap)});
    }
    return rows;
}

bool CsvFormat::TableReader::finished() const { return impl_->reachedEnd; }
bool CsvFormat::TableReader::legacy() const { return impl_->legacyFormat; }
bool CsvFormat::TableReader::needsUpgrade() const { return !impl_->bitmapFormat; }

// Check the table header and deserialize all stored rows.
CsvFormat::TableData CsvFormat::readTable(std::istream& input, const std::string& tableName,
                                         const Schema& schema) const {
    TableReader reader{input, tableName, schema};
    TableData data{{}, reader.legacy(), reader.needsUpgrade()};
    constexpr std::size_t batchSize{1024};
    while (!reader.finished()) {
        auto batch{reader.readNext(batchSize)};
        data.rows.insert(data.rows.end(), std::make_move_iterator(batch.begin()),
                         std::make_move_iterator(batch.end()));
    }
    return data;
}

void CsvFormat::writeTable(std::ostream& output, const Schema& schema,
                            const std::vector<StoredRow>& rows) const {
    std::vector<std::string> header{rowIdHeader, nullBitmapHeader};
    for (const auto& column : schema.columns()) {
        header.push_back(column.name);
    }
    writeCsvRecord(output, header);
    for (const auto& row : rows) {
        writeRow(output, row);
    }
}

void CsvFormat::writePage(std::ostream& output, const Page& page) const {
    for (const auto& row : page.snapshot()) {
        writeRow(output, row);
    }
}

void CsvFormat::writeRow(std::ostream& output, const StoredRow& row) const {
    auto fields{serializeRowForCsv(row.row)};
    fields.insert(fields.begin(), serializeNullBitmap(row.row));
    fields.insert(fields.begin(), std::to_string(row.id));
    writeCsvRecord(output, fields);
}

Schema CsvFormat::readSchema(std::istream& input, const std::string& tableName) const {
    // read catalog header
    std::vector<std::string> fields{};
    if (!readCsvRecord(input, fields) || (fields != catalogHeader && fields != oldCatalogHeader)) {
        throw StorageError{"invalid catalog header for " + tableName};
    }
    const bool oldCatalog{fields == oldCatalogHeader};
    const auto recordWidth{fields.size()};
    std::vector<Column> columns{};
    while (readCsvRecord(input, fields)) {
        if (fields.size() != recordWidth || fields[0] != tableName ||
            fields[1] != std::to_string(columns.size())) {
            throw StorageError{"invalid catalog record for " + tableName};
        }
        if (!oldCatalog && fields[5] != "true" && fields[5] != "false") {
            throw StorageError{"invalid catalog nullability for " + tableName};
        }
        auto column{deserializeColumn(fields[2], fields[3], fields[4])};
        column.nullable = !oldCatalog && fields[5] == "true";
        columns.push_back(std::move(column));
    }
    return Schema{std::move(columns)};
}

void CsvFormat::writeSchema(std::ostream& output, const std::string& tableName,
                             const Schema& schema) const {
    writeCsvRecord(output, catalogHeader);
    for (std::size_t index{0}; index < schema.size(); ++index) {
        const auto& column{schema.column(index)};
        writeCsvRecord(output, {tableName, std::to_string(index), column.name,
                                std::string{dataTypeName(column.type)}, serializeTypeDimension(column),
                                column.nullable ? "true" : "false"});
    }
}

RowId CsvFormat::readNextId(std::istream& input) const {
    std::string value{};
    if (!std::getline(input, value)) {
        throw StorageError{"missing RowId counter"};
    }
    if (input.peek() != std::istream::traits_type::eof() || input.bad()) {
        throw StorageError{"unexpected data after RowId counter"};
    }
    return parseRowId(value, "counter");
}

void CsvFormat::writeNextId(std::ostream& output, RowId id) const {
    output << id << '\n';
}

} // namespace vrdb
