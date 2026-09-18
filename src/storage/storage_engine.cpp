#include "storage/storage_engine.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vrdb {
namespace {

std::string escapeText(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        if (ch == '%' || ch == '|' || ch == '\n' || ch == '\r') {
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

std::string serializeValue(const Value& value) {
    if (const auto* integer = std::get_if<int64_t>(&value)) {
        return "I:" + std::to_string(*integer);
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return "T:" + escapeText(*text);
    }

    const auto& vector = std::get<VectorValue>(value);
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
        throw std::runtime_error("malformed stored value");
    }

    const char tag = encoded[0];
    const std::string payload = encoded.substr(2);

    if (isInteger(column.type) && tag == 'I') {
        return static_cast<std::int64_t>(std::stoll(payload));
    }
    if (isText(column.type) && tag == 'T') {
        return unescapeText(payload);
    }
    if (isVector(column.type) && tag == 'V') {
        VectorValue vector;
        if (!payload.empty()) {
            for (const auto& part : split(payload, ',')) {
                vector.push_back(std::stof(part));
            }
        }
        if (vector.size() != std::get<VectorType>(column.type).dimension()) {
            throw std::runtime_error("stored vector dimension does not match schema");
        }
        return vector;
    }

    throw std::runtime_error("stored value type does not match schema");
}

} // namespace

FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {
    std::filesystem::create_directories(rootDirectory_);
}

void FileStorageEngine::createTable(const std::string& tableName, const Schema& schema) {
    std::filesystem::create_directories(rootDirectory_);

    std::ofstream file(tablePath(tableName), std::ios::trunc);
    if (!file) {
        throw std::runtime_error("failed to create table storage for " + tableName);
    }

    file << "# vrdb table " << tableName << '\n';
    for (const auto& column : schema.columns()) {
        const auto dimension = isVector(column.type)
            ? std::get<VectorType>(column.type).dimension()
            : std::size_t{0};
        file << "# column " << escapeText(column.name) << ' ' << dataTypeName(column.type) << ' '
             << dimension << '\n';
    }
}

void FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    schema.validateRow(row);

    std::ofstream file(tablePath(tableName), std::ios::app);
    if (!file) {
        throw std::runtime_error("failed to append row to " + tableName);
    }

    const auto& values = row.values();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            file << '|';
        }
        file << serializeValue(values[i]);
    }
    file << '\n';
}

std::vector<Row> FileStorageEngine::readRows(const std::string& tableName, const Schema& schema) const {
    std::ifstream file(tablePath(tableName));
    if (!file) {
        throw std::runtime_error("failed to read table storage for " + tableName);
    }

    std::vector<Row> rows;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto parts = split(line, '|');
        if (parts.size() != schema.size()) {
            throw std::runtime_error("stored row width does not match schema");
        }

        std::vector<Value> values;
        values.reserve(parts.size());
        for (std::size_t i = 0; i < parts.size(); ++i) {
            values.push_back(deserializeValue(parts[i], schema.column(static_cast<ColumnId>(i))));
        }

        Row row(std::move(values));
        schema.validateRow(row);
        rows.push_back(std::move(row));
    }

    return rows;
}

std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / (tableName + ".vrdb");
}

} // namespace vrdb
