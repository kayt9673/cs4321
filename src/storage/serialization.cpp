#include "storage/serialization.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace vrdb {
namespace {

// Parse a floating-point coordinate and reject trailing input.
double parseFloat(const std::string& field, const std::string& columnName) {
    try {
        std::size_t parsed{0};
        const auto value{std::stod(field, &parsed)};
        if (parsed != field.size()) {
            throw StorageError{"invalid vector value in column '" + columnName + "'"};
        }
        return value;
    } catch (const StorageError&) {
        throw;
    } catch (const std::exception&) {
        throw StorageError{"invalid vector value in column '" + columnName + "'"};
    }
}

// Parse a bracketed vector and check its declared dimension.
std::vector<double> parseVector(const std::string& field, const Column& column) {
    if (field.size() < 2 || field.front() != '[' || field.back() != ']') {
        throw StorageError{"invalid vector encoding in column '" + column.name + "'"};
    }

    std::vector<double> vector{};
    const auto payload{field.substr(1, field.size() - 2)};
    if (!payload.empty()) {
        if (payload.front() == ',' || payload.back() == ',') {
            throw StorageError{"invalid vector encoding in column '" + column.name + "'"};
        }
        std::stringstream stream{payload};
        std::string part{};
        while (std::getline(stream, part, ',')) {
            if (part.empty()) {
                throw StorageError{"invalid vector encoding in column '" + column.name + "'"};
            }
            vector.push_back(parseFloat(part, column.name));
        }
    }

    const auto expectedDimension{column.vectorDimension};
    if (vector.size() != expectedDimension) {
        throw StorageError{
            "stored vector column '" + column.name + "' expects dimension " +
            std::to_string(expectedDimension) + " but received " +
            std::to_string(vector.size())};
    }
    return vector;
}

} // namespace

// Write one CSV record with quoted fields and escaped quotes.
void writeCsvRecord(std::ostream& output, const std::vector<std::string>& fields) {
    for (std::size_t index{0}; index < fields.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << '"';
        for (const char character : fields[index]) {
            if (character == '"') {
                output << "\"\"";
            } else {
                output << character;
            }
        }
        output << '"';
    }
    output << '\n';
}

// Read one CSV record, returning false at EOF and rejecting malformed quotes.
bool readCsvRecord(std::istream& input, std::vector<std::string>& fields) {
    std::string field{};
    fields.clear();
    if (input.peek() == std::istream::traits_type::eof()) {
        return false;
    }
    bool insideQuotes{false};
    while (input.peek() != std::istream::traits_type::eof() &&
           (insideQuotes || (input.peek() != '\n' && input.peek() != '\r'))) {
        const auto character{input.get()};
        if (character == '"') {
            auto next{input.peek()}; // used to check for escaped quotes and closing quotes
            if (insideQuotes && next == '"') { // escaped quote
                field.push_back('"');
                input.get(); // remove the escaped quote from the buffer
            } else if (insideQuotes) { // closing quote
                fields.push_back(std::move(field));
                field.clear();
                if (next == ',') {
                    input.get(); // remove the separator from the buffer
                    next = input.peek(); // check the next character after the separator
                    if (next != '"') {
                        throw StorageError{"malformed CSV record: expected quoted field after comma"};
                    }
                } else if (next != '\n' && next != '\r' && next != std::istream::traits_type::eof()) {
                    throw StorageError{"malformed CSV record: unexpected character after closing quote"};
                }
                insideQuotes = false;
            } else {
                insideQuotes = true;
            }
        } else {
            if (!insideQuotes) {
                throw StorageError{"malformed CSV record: expected opening quote"};
            }
            field.push_back(static_cast<char>(character));
        }
    }
    if (insideQuotes) {
        throw StorageError{"malformed CSV record: missing closing quote"};
    }
    if (input.peek() == '\r') {
        input.get();
    }
    if (input.peek() == '\n') {
        input.get();
    }

    return true;
}

// Encode a cell as text, preserving double precision in vectors.
std::string serializeCellForCsv(const Cell& cell) {
    const auto type{cellTypeName(cell)};
    if (type == "INTEGER") {
        return std::to_string(std::get<std::int64_t>(cell));
    }
    if (type == "TEXT") {
        return std::get<std::string>(cell);
    }

    const auto& vector{std::get<std::vector<double>>(cell)};
    std::ostringstream output{};
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << '[';
    for (std::size_t index{0}; index < vector.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << vector[index];
    }
    output << ']';
    return output.str();
}

// Decode a CSV field according to its column definition.
Cell deserializeCellFromCsv(const std::string& field, const Column& column) {
    if (isInteger(column.type)) {
        try {
            std::size_t parsed{0};
            const auto value{std::stoll(field, &parsed)};
            if (parsed != field.size()) {
                throw std::invalid_argument{"trailing characters"};
            }
            return static_cast<std::int64_t>(value);
        } catch (const std::exception&) {
            throw StorageError{"invalid integer in column '" + column.name + "'"};
        }
    }
    if (isText(column.type)) {
        return field;
    }
    return parseVector(field, column);
}

// Encode each row cell as a CSV field.
std::vector<std::string> serializeRowForCsv(const Row& row) {
    std::vector<std::string> fields{};
    fields.reserve(row.size());
    for (const auto& cell : row.cells()) {
        fields.push_back(serializeCellForCsv(cell));
    }
    return fields;
}

// Check field count and decode a row using its schema.
Row deserializeRowFromCsv(const std::vector<std::string>& fields, const Schema& schema) {
    if (fields.size() != schema.size()) {
        throw StorageError{
            "stored row expects " + std::to_string(schema.size()) + " fields but received " +
            std::to_string(fields.size())};
    }

    std::vector<Cell> cells{};
    cells.reserve(fields.size());
    for (std::size_t index{0}; index < fields.size(); ++index) {
        cells.push_back(deserializeCellFromCsv(fields[index], schema.column(index)));
    }

    Row row{std::move(cells)};
    schema.validateRow(row);
    return row;
}

// Return the vector dimension as text, or an empty string for other types.
std::string serializeTypeDimension(const Column& column) {
    if (!isVector(column.type)) {
        return {};
    }
    return std::to_string(column.vectorDimension);
}

// Reconstruct and validate a column from catalog fields.
Column deserializeColumn(const std::string& columnName, const std::string& name, const std::string& dimension) {
    if (name == "INTEGER") {
        if (!dimension.empty()) {
            throw StorageError{"INTEGER type must not declare a vector dimension"};
        }
        return Column{columnName, ColumnType::INTEGER};
    }
    if (name == "TEXT") {
        if (!dimension.empty()) {
            throw StorageError{"TEXT type must not declare a vector dimension"};
        }
        return Column{columnName, ColumnType::TEXT};
    }
    if (name == "VECTOR") {
        if (dimension.empty()) {
            throw StorageError{"VECTOR type must declare a dimension"};
        }
        try {
            if (!std::all_of(dimension.begin(), dimension.end(), [](char character) {
                    return character >= '0' && character <= '9';
                })) {
                throw StorageError{"invalid VECTOR dimension: " + dimension};
            }
            const auto value{std::stoull(dimension)};
            if (value > std::numeric_limits<std::size_t>::max()) {
                throw StorageError{"invalid VECTOR dimension: " + dimension};
            }
            return Column{columnName, ColumnType::VECTOR, static_cast<std::size_t>(value)};
        } catch (const DatabaseError&) {
            throw;
        } catch (const std::exception&) {
            throw StorageError{"invalid VECTOR dimension: " + dimension};
        }
    }
    throw StorageError{"unknown data type in catalog: " + name};
}

} // namespace vrdb
