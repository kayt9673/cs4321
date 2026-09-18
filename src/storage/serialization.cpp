#include "storage/serialization.h"

#include "db/errors.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace vrdb {
namespace {

std::string typeName(ColumnType type) {
    switch (type) {
    case ColumnType::INTEGER:
        return "INTEGER";
    case ColumnType::TEXT:
        return "TEXT";
    case ColumnType::VECTOR:
        return "VECTOR";
    }
    throw StorageError("unknown column type");
}

ColumnType parseTypeName(const std::string& type) {
    if (type == "INTEGER") {
        return ColumnType::INTEGER;
    }
    if (type == "TEXT") {
        return ColumnType::TEXT;
    }
    if (type == "VECTOR") {
        return ColumnType::VECTOR;
    }
    throw StorageError("unknown column type in storage: " + type);
}

std::string escapeText(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        if (ch == '%' || ch == '|' || ch == ',' || ch == '\n' || ch == '\r') {
            out << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(ch) << std::nouppercase << std::dec;
        } else {
            out << ch;
        }
    }
    return out.str();
}

std::string unescapeText(const std::string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto code = value.substr(i + 1, 2);
            const char ch = static_cast<char>(std::stoi(code, nullptr, 16));
            out.push_back(ch);
            i += 2;
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

} // namespace

std::string serializeValue(const Value& value) {
    if (const auto* integer = std::get_if<int64_t>(&value)) {
        return "I:" + std::to_string(*integer);
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return "T:" + escapeText(*text);
    }

    const auto& vector = std::get<Vector>(value);
    std::ostringstream out;
    out << "V:";
    for (std::size_t i = 0; i < vector.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << std::setprecision(9) << vector[i];
    }
    return out.str();
}

Value deserializeValue(const std::string& encoded, const Column& column) {
    if (encoded.size() < 2 || encoded[1] != ':') {
        throw StorageError("malformed stored value");
    }

    const char tag = encoded[0];
    const std::string payload = encoded.substr(2);

    if (column.type == ColumnType::INTEGER && tag == 'I') {
        return static_cast<int64_t>(std::stoll(payload));
    }
    if (column.type == ColumnType::TEXT && tag == 'T') {
        return unescapeText(payload);
    }
    if (column.type == ColumnType::VECTOR && tag == 'V') {
        Vector vector;
        if (!payload.empty()) {
            for (const auto& part : split(payload, ',')) {
                vector.push_back(std::stof(part));
            }
        }
        if (!column.vectorDimension || vector.size() != *column.vectorDimension) {
            throw StorageError("stored vector dimension does not match schema");
        }
        return vector;
    }

    throw StorageError("stored value type does not match schema");
}

std::string serializeRow(const Row& row) {
    std::ostringstream out;
    const auto& values = row.values();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << '|';
        }
        out << serializeValue(values[i]);
    }
    return out.str();
}

Row deserializeRow(const std::string& encoded, const Schema& schema) {
    const auto parts = split(encoded, '|');
    if (parts.size() != schema.size()) {
        throw StorageError("stored row width does not match schema");
    }

    std::vector<Value> values;
    values.reserve(parts.size());
    for (std::size_t i = 0; i < parts.size(); ++i) {
        values.push_back(deserializeValue(parts[i], schema.column(i)));
    }

    Row row(std::move(values));
    row.validateAgainst(schema);
    return row;
}

std::string serializeColumn(const Column& column) {
    std::ostringstream out;
    out << escapeText(column.name) << ' ' << typeName(column.type) << ' ';
    out << (column.vectorDimension ? std::to_string(*column.vectorDimension) : "-");
    return out.str();
}

Column deserializeColumn(const std::string& encoded) {
    std::stringstream stream(encoded);
    std::string name;
    std::string type;
    std::string dimension;
    if (!(stream >> name >> type >> dimension)) {
        throw StorageError("malformed stored column");
    }

    const auto columnType = parseTypeName(type);
    if (dimension == "-") {
        return Column(unescapeText(name), columnType);
    }
    return Column(unescapeText(name), columnType, static_cast<std::size_t>(std::stoull(dimension)));
}

} // namespace vrdb
