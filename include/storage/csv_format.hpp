#pragma once

#include "types/row.hpp"
#include "types/schema.hpp"
#include "storage/page.hpp"

#include <istream>
#include <memory>
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

    // Maintain parsing state while a table is consumed in bounded batches.
    class TableReader {
    public:
        TableReader(std::istream& input, std::string tableName, Schema schema);
        ~TableReader();
        TableReader(TableReader&&) noexcept;
        TableReader& operator=(TableReader&&) noexcept;
        TableReader(const TableReader&) = delete;
        TableReader& operator=(const TableReader&) = delete;

        std::vector<StoredRow> readNext(std::size_t maxRows);
        bool finished() const;
        bool legacy() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    TableData readTable(std::istream& input, const std::string& tableName, const Schema& schema) const;
    void writeTable(std::ostream& output, const Schema& schema, const std::vector<StoredRow>& rows) const;
    void writePage(std::ostream& output, const Page& page) const;
    void writeRow(std::ostream& output, const StoredRow& row) const;

    Schema readSchema(std::istream& input, const std::string& tableName) const;
    void writeSchema(std::ostream& output, const std::string& tableName, const Schema& schema) const;

    RowId readNextId(std::istream& input) const;
    void writeNextId(std::ostream& output, RowId id) const;
};

} // namespace vrdb
