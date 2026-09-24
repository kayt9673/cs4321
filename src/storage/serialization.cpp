#include "storage/serialization.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace vrdb {
namespace {

void finishRecord(std::vector<std::string>& fields, std::string& field) {
    fields.push_back(std::move(field));
    field.clear();
}

std::int64_t parseInteger(const std::string& field, const std::string& columnName) {
    try {
        std::size_t parsed = 0;
        const auto value = std::stoll(field, &parsed);
        if (parsed != field.size()) {
            throw StorageError("invalid integer in column '" + columnName + "'");
        }
        return static_cast<std::int64_t>(value);
    } catch (const StorageError&) {
        throw;
    } catch (const std::exception&) {
        throw StorageError("invalid integer in column '" + columnName + "'");
    }
}

double parseFloat(const std::string& field, const std::string& columnName) {
    try {
        std::size_t parsed = 0;
        const auto value = std::stod(field, &parsed);
        if (parsed != field.size()) {
            throw StorageError("invalid vector value in column '" + columnName + "'");
        }
        return value;
    } catch (const StorageError&) {
        throw;
    } catch (const std::exception&) {
        throw StorageError("invalid vector value in column '" + columnName + "'");
    }
}

std::vector<double> parseVector(const std::string& field, const Column& column) {
    if (field.size() < 2 || field.front() != '[' || field.back() != ']') {
        throw StorageError("invalid vector encoding in column '" + column.name + "'");
    }

    std::vector<double> vector;
    const auto payload = field.substr(1, field.size() - 2);
    if (!payload.empty()) {
        if (payload.front() == ',' || payload.back() == ',') {
            throw StorageError("invalid vector encoding in column '" + column.name + "'");
        }
        std::stringstream stream(payload);
        std::string part;
        while (std::getline(stream, part, ',')) {
            if (part.empty()) {
                throw StorageError("invalid vector encoding in column '" + column.name + "'");
            }
            vector.push_back(parseFloat(part, column.name));
        }
    }

    const auto expectedDimension = column.vectorDimension;
    if (vector.size() != expectedDimension) {
        throw StorageError(
            "stored vector column '" + column.name + "' expects dimension " +
            std::to_string(expectedDimension) + " but received " +
            std::to_string(vector.size()));
    }
    return vector;
}

} // namespace

void writeCsvRecord(std::ostream& output, const std::vector<std::string>& fields) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
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

bool readCsvRecord(std::istream& input, std::vector<std::string>& fields) {
    enum class State {
        FIELD_START,
        UNQUOTED,
        QUOTED,
        AFTER_QUOTE
    };

    fields.clear();
    std::string field;
    State state = State::FIELD_START;
    bool readAnything = false;
    char character = '\0';

    while (input.get(character)) {
        readAnything = true;

        if (state == State::QUOTED) {
            if (character == '"') {
                if (input.peek() == '"') {
                    input.get(character);
                    field.push_back('"');
                } else {
                    state = State::AFTER_QUOTE;
                }
            } else {
                field.push_back(character);
            }
            continue;
        }

        const bool recordEnd = character == '\n' || character == '\r';
        if (recordEnd) {
            if (character == '\r' && input.peek() == '\n') {
                input.get(character);
            }
            finishRecord(fields, field);
            return true;
        }

        if (state == State::AFTER_QUOTE) {
            if (character != ',') {
                throw StorageError("malformed CSV record: unexpected character after closing quote");
            }
            finishRecord(fields, field);
            state = State::FIELD_START;
            continue;
        }

        if (character == ',') {
            finishRecord(fields, field);
            state = State::FIELD_START;
        } else if (character == '"') {
            if (state != State::FIELD_START) {
                throw StorageError("malformed CSV record: quote inside unquoted field");
            }
            state = State::QUOTED;
        } else {
            field.push_back(character);
            state = State::UNQUOTED;
        }
    }

    if (!readAnything) {
        return false;
    }
    if (state == State::QUOTED) {
        throw StorageError("malformed CSV record: unterminated quoted field");
    }
    finishRecord(fields, field);
    return true;
}

std::string serializeCellForCsv(const Cell& cell) {
    if (const auto* integer = std::get_if<std::int64_t>(&cell)) {
        return std::to_string(*integer);
    }
    if (const auto* text = std::get_if<std::string>(&cell)) {
        return *text;
    }

    const auto& vector = std::get<std::vector<double>>(cell);
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < vector.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << std::setprecision(std::numeric_limits<double>::max_digits10) << vector[index];
    }
    output << ']';
    return output.str();
}

Cell deserializeCellFromCsv(const std::string& field, const Column& column) {
    if (isInteger(column.type)) {
        return parseInteger(field, column.name);
    }
    if (isText(column.type)) {
        return field;
    }
    return parseVector(field, column);
}

std::vector<std::string> serializeRowForCsv(const Row& row) {
    std::vector<std::string> fields;
    fields.reserve(row.size());
    for (const auto& cell : row.cells()) {
        fields.push_back(serializeCellForCsv(cell));
    }
    return fields;
}

Row deserializeRowFromCsv(const std::vector<std::string>& fields, const Schema& schema) {
    if (fields.size() != schema.size()) {
        throw StorageError(
            "stored row expects " + std::to_string(schema.size()) + " fields but received " +
            std::to_string(fields.size()));
    }

    std::vector<Cell> cells;
    cells.reserve(fields.size());
    for (std::size_t index = 0; index < fields.size(); ++index) {
        cells.push_back(deserializeCellFromCsv(fields[index], schema.column(static_cast<ColumnId>(index))));
    }

    Row row(std::move(cells));
    schema.validateRow(row);
    return row;
}

std::string serializeTypeDimension(const Column& column) {
    if (!isVector(column.type)) {
        return {};
    }
    return std::to_string(column.vectorDimension);
}

Column deserializeColumn(const std::string& columnName, const std::string& name, const std::string& dimension) {
    if (name == "INTEGER") {
        if (!dimension.empty()) {
            throw StorageError("INTEGER type must not declare a vector dimension");
        }
        return Column(columnName, ColumnType::INTEGER);
    }
    if (name == "TEXT") {
        if (!dimension.empty()) {
            throw StorageError("TEXT type must not declare a vector dimension");
        }
        return Column(columnName, ColumnType::TEXT);
    }
    if (name == "VECTOR") {
        if (dimension.empty()) {
            throw StorageError("VECTOR type must declare a dimension");
        }
        try {
            if (!std::all_of(dimension.begin(), dimension.end(), [](char character) {
                    return character >= '0' && character <= '9';
                })) {
                throw StorageError("invalid VECTOR dimension: " + dimension);
            }
            std::size_t parsed = 0;
            const auto value = std::stoull(dimension, &parsed);
            if (parsed != dimension.size() || value > std::numeric_limits<std::size_t>::max()) {
                throw StorageError("invalid VECTOR dimension: " + dimension);
            }
            return Column(columnName, ColumnType::VECTOR, static_cast<std::size_t>(value));
        } catch (const DatabaseError&) {
            throw;
        } catch (const std::exception&) {
            throw StorageError("invalid VECTOR dimension: " + dimension);
        }
    }
    throw StorageError("unknown data type in catalog: " + name);
}

} // namespace vrdb
