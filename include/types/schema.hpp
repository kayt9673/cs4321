#pragma once

#include "types/column.hpp"
#include "types/cell.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vrdb {

class Row;

class Schema {
public:
    // Validate columns and build the column-name lookup map.
    explicit Schema(std::vector<Column> columns);

    // Return the schema columns in declaration order.
    const std::vector<Column>& columns() const;
    // Return a column by ID or name, rejecting missing columns.
    const Column& column(std::size_t id) const;
    // Return a column by ID or name, rejecting missing columns.
    const Column& column(std::string_view name) const;
    // Return the number of columns in the schema.
    std::size_t size() const;
    // Return whether the schema contains no columns.
    bool empty() const;

    // Return whether the schema contains the given column name.
    bool hasColumn(std::string_view name) const;
    // Resolve a column name to its ID, or return no value if absent.
    std::optional<std::size_t> columnId(std::string_view name) const;
    // Check that a cell matches its column type and vector dimension.
    void validateCell(std::size_t column, const Cell& cell) const;
    // Check row width and validate each cell against the schema.
    void validateRow(const Row& row) const;

private:
    std::vector<Column> columns_;
    std::unordered_map<std::string, std::size_t> nameToId_;
};

} // namespace vrdb
