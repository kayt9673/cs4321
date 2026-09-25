#include "db/database.h"
#include "storage/serialization.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

// Colors are emitted only when the stream is a terminal, so piped output and
// tests stay plain. NO_COLOR disables them; CLICOLOR_FORCE forces them on.
bool colorEnabledFor(int fileDescriptor) {
    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }
    if (const char* force = std::getenv("CLICOLOR_FORCE"); force != nullptr && std::string(force) != "0") {
        return true;
    }
    if (const char* term = std::getenv("TERM"); term != nullptr && std::string(term) == "dumb") {
        return false;
    }
    return isatty(fileDescriptor) != 0;
}

bool stdoutColor = false;
bool stderrColor = false;

namespace ansi {
constexpr const char* reset = "\033[0m";
constexpr const char* bold = "\033[1m";
constexpr const char* dim = "\033[2m";
constexpr const char* red = "\033[1;31m";
constexpr const char* green = "\033[32m";
constexpr const char* yellow = "\033[33m";
constexpr const char* magenta = "\033[35m";
constexpr const char* cyan = "\033[36m";
} // namespace ansi

std::string paint(const std::string& text, const char* style, bool enabled = stdoutColor) {
    return enabled ? std::string(style) + text + ansi::reset : text;
}

const char* typeColor(const vrdb::DataType& type) {
    if (vrdb::isInteger(type)) {
        return ansi::cyan;
    }
    if (vrdb::isText(type)) {
        return ansi::yellow;
    }
    return ansi::magenta;
}

std::string typeLabel(const vrdb::DataType& type) {
    std::string label(vrdb::dataTypeName(type));
    if (vrdb::isVector(type)) {
        label += '(' + std::to_string(std::get<vrdb::VectorType>(type).dimension()) + ')';
    }
    return label;
}

void printSuccess(const std::string& message, const std::string& subject) {
    if (stdoutColor) {
        std::cout << paint("\u2713 ", ansi::green) << message << ' ' << paint(subject, ansi::bold) << '\n';
    } else {
        std::cout << message << ' ' << subject << '\n';
    }
}

void printUsage(std::ostream& output) {
    output
        << "Usage:\n"
        << "  vrdb_cli <database-directory> init\n"
        << "  vrdb_cli <database-directory> list\n"
        << "  vrdb_cli <database-directory> describe <table>\n"
        << "  vrdb_cli <database-directory> create <table> <column:type>...\n"
        << "  vrdb_cli <database-directory> insert <table> <value>...\n"
        << "  vrdb_cli <database-directory> select <table> [--pretty]\n\n"
        << "Column types: INTEGER, TEXT, VECTOR(n)\n"
        << "Vector values: [0.1,0.2,0.3]\n"
        << "select prints CSV by default; --pretty prints a formatted table.\n"
        << "Terminal output is colored unless NO_COLOR is set.\n";
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

std::size_t parseSize(const std::string& value, const std::string& description) {
    try {
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            })) {
            throw std::invalid_argument("non-digit character");
        }
        std::size_t parsed = 0;
        const auto result = std::stoull(value, &parsed);
        if (parsed != value.size() || result > std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::size_t>(result);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid " + description + ": " + value);
    }
}

vrdb::DataType parseDataType(std::string type) {
    type = uppercase(std::move(type));
    if (type == "INTEGER") {
        return vrdb::Int64Type{};
    }
    if (type == "TEXT") {
        return vrdb::TextType{};
    }
    if (type.size() > 8 && type.rfind("VECTOR(", 0) == 0 && type.back() == ')') {
        return vrdb::VectorType{parseSize(type.substr(7, type.size() - 8), "vector dimension")};
    }
    throw std::invalid_argument("unknown column type: " + type);
}

vrdb::Column parseColumn(const std::string& specification) {
    const auto separator = specification.find(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 == specification.size()) {
        throw std::invalid_argument("invalid column specification: " + specification);
    }
    return vrdb::Column(
        specification.substr(0, separator),
        parseDataType(specification.substr(separator + 1)));
}

std::int64_t parseInteger(const std::string& input) {
    try {
        std::size_t parsed = 0;
        const auto value = std::stoll(input, &parsed);
        if (parsed != input.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::int64_t>(value);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid INTEGER value: " + input);
    }
}

vrdb::VectorValue parseVector(const std::string& input) {
    if (input.size() < 2 || input.front() != '[' || input.back() != ']') {
        throw std::invalid_argument("invalid VECTOR value: " + input);
    }

    vrdb::VectorValue vector;
    const auto payload = input.substr(1, input.size() - 2);
    if (!payload.empty() && (payload.front() == ',' || payload.back() == ',')) {
        throw std::invalid_argument("invalid VECTOR value: " + input);
    }
    std::size_t begin = 0;
    while (begin < payload.size()) {
        const auto end = payload.find(',', begin);
        const auto part = payload.substr(begin, end == std::string::npos ? end : end - begin);
        try {
            std::size_t parsed = 0;
            const auto value = std::stof(part, &parsed);
            if (parsed != part.size()) {
                throw std::invalid_argument("trailing characters");
            }
            vector.push_back(value);
        } catch (const std::exception&) {
            throw std::invalid_argument("invalid VECTOR value: " + input);
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return vector;
}

vrdb::Value parseValue(const std::string& input, const vrdb::Column& column) {
    if (vrdb::isInteger(column.type)) {
        return parseInteger(input);
    }
    if (vrdb::isText(column.type)) {
        return input;
    }
    return parseVector(input);
}

void printSchema(const vrdb::Schema& schema) {
    std::size_t nameWidth = 0;
    for (const auto& column : schema.columns()) {
        nameWidth = std::max(nameWidth, column.name.size());
    }
    for (const auto& column : schema.columns()) {
        std::cout << paint(column.name, ansi::bold) << std::string(nameWidth - column.name.size() + 1, ' ')
                  << paint(typeLabel(column.type), typeColor(column.type)) << '\n';
    }
}

// Number of terminal columns a UTF-8 string occupies, assuming no wide glyphs.
std::size_t displayWidth(const std::string& text) {
    return static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [](char character) {
        return (static_cast<unsigned char>(character) & 0xC0) != 0x80;
    }));
}

std::string truncateForDisplay(const std::string& text, std::size_t maxWidth) {
    if (displayWidth(text) <= maxWidth) {
        return text;
    }
    std::string result;
    std::size_t width = 0;
    for (const char character : text) {
        const bool startsCodePoint = (static_cast<unsigned char>(character) & 0xC0) != 0x80;
        if (startsCodePoint && ++width > maxWidth - 1) {
            break;
        }
        result += character;
    }
    return result + "\u2026";
}

std::string formatCell(const vrdb::Value& value) {
    constexpr std::size_t maxTextWidth = 40;
    constexpr std::size_t maxVectorElements = 6;

    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*integer);
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        std::string singleLine;
        for (const char character : *text) {
            if (character == '\n') {
                singleLine += "\u21b5";
            } else if (character == '\r' || character == '\t') {
                singleLine += ' ';
            } else {
                singleLine += character;
            }
        }
        return truncateForDisplay(singleLine, maxTextWidth);
    }

    const auto& vector = std::get<vrdb::VectorValue>(value);
    const bool elide = vector.size() > maxVectorElements;
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < vector.size(); ++index) {
        if (elide && index == maxVectorElements - 1) {
            output << ", \u2026";
            index = vector.size() - 1;
        }
        if (index > 0) {
            output << ", ";
        }
        output << vector[index];
    }
    output << ']';
    if (elide) {
        output << " (" << vector.size() << "d)";
    }
    return output.str();
}

void printPrettyResult(const vrdb::QueryResult& result) {
    const auto& columns = result.schema.columns();
    std::vector<std::vector<std::string>> cells;
    cells.reserve(result.rows.size());
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    for (const auto& column : columns) {
        widths.push_back(displayWidth(column.name));
    }
    for (const auto& row : result.rows) {
        auto& formatted = cells.emplace_back();
        for (std::size_t index = 0; index < row.size(); ++index) {
            formatted.push_back(formatCell(row.values()[index]));
            widths[index] = std::max(widths[index], displayWidth(formatted.back()));
        }
    }

    const auto border = [&](const char* left, const char* middle, const char* right) {
        std::string line = left;
        for (std::size_t index = 0; index < widths.size(); ++index) {
            if (index > 0) {
                line += middle;
            }
            for (std::size_t count = 0; count < widths[index] + 2; ++count) {
                line += "\u2500";
            }
        }
        std::cout << paint(line + right, ansi::dim) << '\n';
    };
    const auto printRow = [&](const std::vector<std::string>& values, bool header) {
        const auto separator = paint("\u2502", ansi::dim);
        std::cout << separator;
        for (std::size_t index = 0; index < values.size(); ++index) {
            const auto padding = std::string(widths[index] - displayWidth(values[index]), ' ');
            const bool rightAlign = !header && vrdb::isInteger(columns[index].type);
            const auto styled = paint(values[index], header ? ansi::bold : typeColor(columns[index].type));
            std::cout << ' ' << (rightAlign ? padding + styled : styled + padding) << ' ' << separator;
        }
        std::cout << '\n';
    };

    std::vector<std::string> header;
    header.reserve(columns.size());
    for (const auto& column : columns) {
        header.push_back(column.name);
    }

    border("\u250c", "\u252c", "\u2510");
    printRow(header, true);
    border("\u251c", "\u253c", "\u2524");
    for (const auto& row : cells) {
        printRow(row, false);
    }
    border("\u2514", "\u2534", "\u2518");
    const auto count = result.rows.size();
    std::cout << paint(std::to_string(count) + (count == 1 ? " row" : " rows"), ansi::dim) << '\n';
}

void printQueryResult(const vrdb::QueryResult& result) {
    std::vector<std::string> header;
    header.reserve(result.schema.size());
    for (const auto& column : result.schema.columns()) {
        header.push_back(column.name);
    }
    vrdb::writeCsvRecord(std::cout, header);
    for (const auto& row : result.rows) {
        vrdb::writeCsvRecord(std::cout, vrdb::serializeRowForCsv(row));
    }
}

} // namespace

int main(int argc, char** argv) {
    stdoutColor = colorEnabledFor(STDOUT_FILENO);
    stderrColor = colorEnabledFor(STDERR_FILENO);

    if (argc < 3) {
        printUsage(std::cerr);
        return 2;
    }

    try {
        const std::string databasePath = argv[1];
        const std::string command = argv[2];
        vrdb::Database database(databasePath);

        if (command == "init") {
            if (argc != 3) {
                throw std::invalid_argument("init does not accept additional arguments");
            }
            printSuccess("Initialized database at", databasePath);
            return 0;
        }

        if (command == "list") {
            if (argc != 3) {
                throw std::invalid_argument("list does not accept additional arguments");
            }
            for (const auto& tableName : database.listTables()) {
                std::cout << paint(tableName, ansi::bold) << '\n';
            }
            return 0;
        }

        if (command == "describe") {
            if (argc != 4) {
                throw std::invalid_argument("describe requires exactly one table name");
            }
            printSchema(database.getSchema(argv[3]));
            return 0;
        }

        if (command == "create") {
            if (argc < 5) {
                throw std::invalid_argument("create requires a table name and at least one column");
            }
            std::vector<vrdb::Column> columns;
            columns.reserve(static_cast<std::size_t>(argc - 4));
            for (int index = 4; index < argc; ++index) {
                columns.push_back(parseColumn(argv[index]));
            }
            database.createTable(argv[3], vrdb::Schema(std::move(columns)));
            printSuccess("Created table", argv[3]);
            return 0;
        }

        if (command == "insert") {
            if (argc < 4) {
                throw std::invalid_argument("insert requires a table name");
            }
            const auto& schema = database.getSchema(argv[3]);
            if (static_cast<std::size_t>(argc - 4) != schema.size()) {
                throw std::invalid_argument(
                    "insert expects " + std::to_string(schema.size()) + " values but received " +
                    std::to_string(argc - 4));
            }

            std::vector<vrdb::Value> values;
            values.reserve(schema.size());
            for (std::size_t index = 0; index < schema.size(); ++index) {
                const auto column = static_cast<vrdb::ColumnId>(index);
                values.push_back(parseValue(argv[static_cast<int>(index) + 4], schema.column(column)));
            }
            database.insert(argv[3], vrdb::Row(std::move(values)));
            printSuccess("Inserted 1 row into", argv[3]);
            return 0;
        }

        if (command == "select") {
            const bool pretty = argc == 5 && std::string(argv[4]) == "--pretty";
            if (argc != 4 && !pretty) {
                throw std::invalid_argument("select requires exactly one table name and optionally --pretty");
            }
            vrdb::Query query;
            query.table = argv[3];
            const auto result = database.select(query);
            if (pretty) {
                printPrettyResult(result);
            } else {
                printQueryResult(result);
            }
            return 0;
        }

        throw std::invalid_argument("unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << paint("Error:", ansi::red, stderrColor) << ' ' << error.what() << '\n';
        return 1;
    }
}
