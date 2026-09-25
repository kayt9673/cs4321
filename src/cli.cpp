#include "db/database.hpp"
#include "storage/serialization.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

// Print supported CLI commands and argument formats.
void printUsage(std::ostream& output) {
    output
        << "Usage:\n"
        << "  vrdb_cli <database-directory> init\n"
        << "  vrdb_cli <database-directory> list\n"
        << "  vrdb_cli <database-directory> describe <table>\n"
        << "  vrdb_cli <database-directory> create <table> <column:type>...\n"
        << "  vrdb_cli <database-directory> insert <table> <cell>...\n"
        << "  vrdb_cli <database-directory> select <table>\n\n"
        << "Column types: INTEGER, TEXT, VECTOR(n)\n"
        << "Vector cells: [0.1,0.2,0.3]\n";
}

// Convert a string to uppercase for case-insensitive type parsing.
std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

// Parse a nonnegative size and reject malformed or overflowing input.
std::size_t parseSize(const std::string& value, const std::string& description) {
    try {
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            })) {
            throw std::invalid_argument{"non-digit character"};
        }
        const auto result{std::stoull(value)};
        if (result > std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument{"value out of range"};
        }
        return static_cast<std::size_t>(result);
    } catch (const std::exception&) {
        throw std::invalid_argument{"invalid " + description + ": " + value};
    }
}

// Parse a name:type argument into a validated column definition.
vrdb::Column parseColumn(const std::string& specification) {
    const auto separator{specification.find(':')};
    if (separator == std::string::npos || separator == 0 || separator + 1 == specification.size()) {
        throw std::invalid_argument{"invalid column specification: " + specification};
    }
    const auto name{specification.substr(0, separator)};
    const auto type{uppercase(specification.substr(separator + 1))};
    if (type == "INTEGER") {
        return vrdb::Column{name, vrdb::ColumnType::INTEGER};
    }
    if (type == "TEXT") {
        return vrdb::Column{name, vrdb::ColumnType::TEXT};
    }
    if (type.size() > 8 && type.rfind("VECTOR(", 0) == 0 && type.back() == ')') {
        return vrdb::Column{name, vrdb::ColumnType::VECTOR,
                            parseSize(type.substr(7, type.size() - 8), "vector dimension")};
    }
    throw std::invalid_argument{"unknown column type: " + type};
}

// Parse a signed integer and reject invalid or trailing input.
std::int64_t parseInteger(const std::string& input) {
    try {
        std::size_t parsed{0};
        const auto value{std::stoll(input, &parsed)};
        if (parsed != input.size()) {
            throw std::invalid_argument{"trailing characters"};
        }
        return static_cast<std::int64_t>(value);
    } catch (const std::exception&) {
        throw std::invalid_argument{"invalid INTEGER cell: " + input};
    }
}

// Parse a bracketed list of floating-point coordinates.
std::vector<double> parseVector(const std::string& input) {
    if (input.size() < 2 || input.front() != '[' || input.back() != ']') {
        throw std::invalid_argument{"invalid VECTOR cell: " + input};
    }

    std::vector<double> vector{};
    const auto payload{input.substr(1, input.size() - 2)};
    if (!payload.empty() && (payload.front() == ',' || payload.back() == ',')) {
        throw std::invalid_argument{"invalid VECTOR cell: " + input};
    }
    std::size_t begin{0};
    while (begin < payload.size()) {
        const auto end{payload.find(',', begin)};
        const auto part{payload.substr(begin, end == std::string::npos ? end : end - begin)};
        try {
            std::size_t parsed{0};
            const auto value{std::stod(part, &parsed)};
            if (parsed != part.size()) {
                throw std::invalid_argument{"trailing characters"};
            }
            vector.push_back(value);
        } catch (const std::exception&) {
            throw std::invalid_argument{"invalid VECTOR cell: " + input};
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return vector;
}

// Parse a CLI cell according to its column type.
vrdb::Cell parseCell(const std::string& input, const vrdb::Column& column) {
    if (vrdb::isInteger(column.type)) {
        return parseInteger(input);
    }
    if (vrdb::isText(column.type)) {
        return input;
    }
    return parseVector(input);
}

// Print column names, types, and vector dimensions.
void printSchema(const vrdb::Schema& schema) {
    for (const auto& column : schema.columns()) {
        std::cout << column.name << ' ' << vrdb::columnTypeName(column.type);
        if (vrdb::isVector(column.type)) {
            std::cout << '(' << column.vectorDimension << ')';
        }
        std::cout << '\n';
    }
}

// Print the result header and rows as CSV.
void printQueryResult(const vrdb::QueryResult& result) {
    std::vector<std::string> header{};
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

// Parse CLI arguments, execute one database command, and report errors.
int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage(std::cerr);
        return 2;
    }

    try {
        const std::string databasePath{argv[1]};
        const std::string command{argv[2]};
        vrdb::DatabaseManager database{databasePath};

        if (command == "init") {
            if (argc != 3) {
                throw std::invalid_argument{"init does not accept additional arguments"};
            }
            std::cout << "Initialized database at " << databasePath << '\n';
            return 0;
        }

        if (command == "list") {
            if (argc != 3) {
                throw std::invalid_argument{"list does not accept additional arguments"};
            }
            for (const auto& tableName : database.listTables()) {
                std::cout << tableName << '\n';
            }
            return 0;
        }

        if (command == "describe") {
            if (argc != 4) {
                throw std::invalid_argument{"describe requires exactly one table name"};
            }
            printSchema(database.getSchema(argv[3]));
            return 0;
        }

        if (command == "create") {
            if (argc < 5) {
                throw std::invalid_argument{"create requires a table name and at least one column"};
            }
            std::vector<vrdb::Column> columns{};
            columns.reserve(static_cast<std::size_t>(argc - 4));
            for (int index{4}; index < argc; ++index) {
                columns.push_back(parseColumn(argv[index]));
            }
            database.createTable(argv[3], vrdb::Schema{std::move(columns)});
            std::cout << "Created table " << argv[3] << '\n';
            return 0;
        }

        if (command == "insert") {
            if (argc < 4) {
                throw std::invalid_argument{"insert requires a table name"};
            }
            const auto& schema{database.getSchema(argv[3])};
            if (static_cast<std::size_t>(argc - 4) != schema.size()) {
                throw std::invalid_argument{
                    "insert expects " + std::to_string(schema.size()) + " cells but received " +
                    std::to_string(argc - 4)};
            }

            std::vector<vrdb::Cell> cells{};
            cells.reserve(schema.size());
            for (std::size_t index{0}; index < schema.size(); ++index) {
                const auto column{static_cast<vrdb::ColumnId>(index)};
                cells.push_back(parseCell(argv[static_cast<int>(index) + 4], schema.column(column)));
            }
            database.insert(argv[3], vrdb::Row{std::move(cells)});
            std::cout << "Inserted 1 row into " << argv[3] << '\n';
            return 0;
        }

        if (command == "select") {
            if (argc != 4) {
                throw std::invalid_argument{"select requires exactly one table name"};
            }
            vrdb::Query query{};
            query.table = argv[3];
            printQueryResult(database.select(query));
            return 0;
        }

        throw std::invalid_argument{"unknown command: " + command};
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
