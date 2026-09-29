#pragma once

#include "types/row.hpp"
#include "types/schema.hpp"

#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace vrdb {

// Encode tables and schemas without opening files or choosing their paths.
class CsvFormat {
public:
    static constexpr const char* extension{".csv"};

    struct TableData {
        std::vector<StoredRow> rows;
        bool legacy;
    };

    TableData readTable(std::istream& input, const std::string& tableName, const Schema& schema) const;
    void writeTable(std::ostream& output, const Schema& schema, const std::vector<StoredRow>& rows) const;
    void writeRow(std::ostream& output, const StoredRow& row) const;

    Schema readSchema(std::istream& input, const std::string& tableName) const;
    void writeSchema(std::ostream& output, const std::string& tableName, const Schema& schema) const;

    RowId readNextId(std::istream& input) const;
    void writeNextId(std::ostream& output, RowId id) const;
};

} // namespace vrdb
