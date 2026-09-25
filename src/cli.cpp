#include "db/database.h"
#include "storage/serialization.h"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void printUsage(std::ostream& output) {
    output
        << "Usage:\n"
        << "  vrdb_cli <database-directory> init\n"
        << "  vrdb_cli <database-directory> list\n"
        << "  vrdb_cli <database-directory> describe <table>\n"
        << "  vrdb_cli <database-directory> create <table> <column:type>...\n"
        << "  vrdb_cli <database-directory> insert <table> <value>...\n"
        << "  vrdb_cli <database-directory> update <table> <row-id> <value>...\n"
        << "  vrdb_cli <database-directory> delete <table> <row-id>\n"
        << "  vrdb_cli <database-directory> select <table> [--csv]\n\n"
        << "Column types: INTEGER, TEXT, VECTOR(n)\n"
        << "Vector values: [0.1,0.2,0.3]\n";
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

vrdb::RowId parseRowId(const std::string& value) {
    const auto parsed = parseSize(value, "RowId");
    if (parsed == 0 || parsed > std::numeric_limits<vrdb::RowId>::max()) {
        throw std::invalid_argument("invalid RowId: " + value);
    }
    return static_cast<vrdb::RowId>(parsed);
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

vrdb::Row parseRowValues(const vrdb::Schema& schema, int argc, char** argv, int firstValue,
                        const std::string& command) {
    if (static_cast<std::size_t>(argc - firstValue) != schema.size()) {
        throw std::invalid_argument(
            command + " expects " + std::to_string(schema.size()) + " values but received " +
            std::to_string(argc - firstValue));
    }
    std::vector<vrdb::Value> values;
    values.reserve(schema.size());
    for (std::size_t index = 0; index < schema.size(); ++index) {
        values.push_back(parseValue(argv[firstValue + static_cast<int>(index)],
                                    schema.column(static_cast<vrdb::ColumnId>(index))));
    }
    return vrdb::Row(std::move(values));
}

bool useColor(bool errorOutput = false) {
    if (std::getenv("NO_COLOR") || (std::getenv("TERM") && std::string(std::getenv("TERM")) == "dumb")) {
        return false;
    }
#if defined(_WIN32)
    return _isatty(_fileno(errorOutput ? stderr : stdout)) != 0;
#else
    return isatty(errorOutput ? STDERR_FILENO : STDOUT_FILENO) != 0;
#endif
}

std::string escapeCell(const std::string& value) {
    std::string output;
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned char character : value) {
        if (character == '\n') output += "\\n";
        else if (character == '\r') output += "\\r";
        else if (character == '\t') output += "\\t";
        else if (character < 0x20 || character == 0x7f) {
            output += "\\x";
            output += digits[character >> 4];
            output += digits[character & 0x0f];
        }
        else output += static_cast<char>(character);
    }
    return output;
}

std::size_t displayWidth(const std::string& value) {
#if defined(_WIN32)
    return value.size();
#else
    std::mbstate_t state{};
    const char* cursor = value.data();
    std::size_t remaining = value.size();
    std::size_t width = 0;
    while (remaining > 0) {
        wchar_t character = 0;
        const auto consumed = std::mbrtowc(&character, cursor, remaining, &state);
        if (consumed == static_cast<std::size_t>(-1) || consumed == static_cast<std::size_t>(-2)) {
            ++cursor;
            --remaining;
            ++width;
            state = std::mbstate_t{};
            continue;
        }
        const auto bytes = consumed == 0 ? 1 : consumed;
        const auto cells = ::wcwidth(character);
        width += cells < 0 ? 1 : static_cast<std::size_t>(cells);
        cursor += bytes;
        remaining -= bytes;
    }
    return width;
#endif
}

void printTable(const std::vector<std::string>& header, const std::vector<std::vector<std::string>>& rows) {
    std::vector<std::size_t> widths;
    widths.reserve(header.size());
    for (const auto& name : header) widths.push_back(displayWidth(escapeCell(name)));
    for (const auto& row : rows) {
        for (std::size_t index = 0; index < row.size(); ++index) {
            widths[index] = std::max(widths[index], displayWidth(escapeCell(row[index])));
        }
    }
    const auto border = [&]() {
        std::cout << '+';
        for (const auto width : widths) std::cout << std::string(width + 2, '-') << '+';
        std::cout << '\n';
    };
    const bool color = useColor();
    const auto line = [&](const std::vector<std::string>& cells, bool heading) {
        std::cout << '|';
        for (std::size_t index = 0; index < cells.size(); ++index) {
            const auto display = escapeCell(cells[index]);
            std::cout << ' ';
            const bool styled = color && (heading || index == 0);
            if (color && heading) std::cout << "\x1b[1;36m";
            else if (color && index == 0) std::cout << "\x1b[33m";
            std::cout << display;
            if (styled) std::cout << "\x1b[0m";
            std::cout << std::string(widths[index] - displayWidth(display) + 1, ' ') << '|';
        }
        std::cout << '\n';
    };
    border();
    line(header, true);
    border();
    for (const auto& row : rows) line(row, false);
    border();
    std::cout << rows.size() << (rows.size() == 1 ? " row" : " rows") << '\n';
}

void printSuccess(const std::string& message) {
    if (useColor()) std::cout << "\x1b[1;32m";
    std::cout << message;
    if (useColor()) std::cout << "\x1b[0m";
    std::cout << '\n';
}

void printSchema(const vrdb::Schema& schema) {
    std::vector<std::vector<std::string>> rows;
    for (const auto& column : schema.columns()) {
        std::string type(vrdb::dataTypeName(column.type));
        if (vrdb::isVector(column.type)) {
            type += '(' + std::to_string(std::get<vrdb::VectorType>(column.type).dimension()) + ')';
        }
        rows.push_back({column.name, type});
    }
    printTable({"column", "type"}, rows);
}

void printQueryResult(const vrdb::QueryResult& result, bool csv) {
    std::vector<std::string> header{"row_id"};
    header.reserve(result.schema.size() + 1);
    for (const auto& column : result.schema.columns()) {
        header.push_back(column.name);
    }
    std::vector<std::vector<std::string>> rows;
    rows.reserve(result.rows.size());
    for (std::size_t index = 0; index < result.rows.size(); ++index) {
        auto fields = vrdb::serializeRowForCsv(result.rows[index]);
        fields.insert(fields.begin(), std::to_string(result.rowIds[index]));
        rows.push_back(std::move(fields));
    }
    if (csv) {
        vrdb::writeCsvRecord(std::cout, header);
        for (const auto& row : rows) vrdb::writeCsvRecord(std::cout, row);
    } else {
        printTable(header, rows);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::setlocale(LC_CTYPE, "");
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
            printSuccess("Initialized database at " + databasePath);
            return 0;
        }

        if (command == "list") {
            if (argc != 3) {
                throw std::invalid_argument("list does not accept additional arguments");
            }
            std::vector<std::vector<std::string>> rows;
            for (const auto& tableName : database.listTables()) rows.push_back({tableName});
            printTable({"table"}, rows);
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
            printSuccess("Created table " + std::string(argv[3]));
            return 0;
        }

        if (command == "insert") {
            if (argc < 4) {
                throw std::invalid_argument("insert requires a table name");
            }
            const auto& schema = database.getSchema(argv[3]);
            const auto id = database.insert(argv[3], parseRowValues(schema, argc, argv, 4, "insert"));
            printSuccess("Inserted row " + std::to_string(id) + " into " + argv[3]);
            return 0;
        }

        if (command == "update") {
            if (argc < 5) throw std::invalid_argument("update requires a table name and RowId");
            const auto& schema = database.getSchema(argv[3]);
            const auto id = parseRowId(argv[4]);
            database.update(argv[3], id, parseRowValues(schema, argc, argv, 5, "update"));
            printSuccess("Updated row " + std::to_string(id) + " in " + argv[3]);
            return 0;
        }

        if (command == "delete") {
            if (argc != 5) throw std::invalid_argument("delete requires a table name and RowId");
            const auto id = parseRowId(argv[4]);
            database.erase(argv[3], id);
            printSuccess("Deleted row " + std::to_string(id) + " from " + argv[3]);
            return 0;
        }

        if (command == "select") {
            if (argc != 4 && !(argc == 5 && std::string(argv[4]) == "--csv")) {
                throw std::invalid_argument("select requires a table name and optional --csv");
            }
            vrdb::Query query;
            query.table = argv[3];
            printQueryResult(database.select(query), argc == 5);
            return 0;
        }

        throw std::invalid_argument("unknown command: " + command);
    } catch (const std::exception& error) {
        if (useColor(true)) std::cerr << "\x1b[1;31m";
        std::cerr << "Error: " << error.what();
        if (useColor(true)) std::cerr << "\x1b[0m";
        std::cerr << '\n';
        return 1;
    }
}
