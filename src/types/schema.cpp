#include "types/schema.hpp"

#include "db/errors.hpp"
#include "types/row.hpp"

#include <utility>

namespace vrdb {

// Validate columns and build the column-name lookup map.
Schema::Schema(std::vector<Column> columns)
    : columns_{std::move(columns)} {
    if (columns_.empty()) {
        throw SchemaError{"schema must contain at least one column"};
    }

    nameToId_.reserve(columns_.size());
    for (std::size_t index{0}; index < columns_.size(); ++index) {
        const auto& column{columns_[index]};
        column.validate();
        const auto id{index};
        const auto& name{columns_[index].name};
        if (!nameToId_.emplace(name, id).second) {
            throw SchemaError{"duplicate column name: " + name};
        }
    }
}

// Return the schema columns in declaration order.
const std::vector<Column>& Schema::columns() const {
    return columns_;
}

// Return a column by ID or name, rejecting missing columns.
const Column& Schema::column(std::size_t id) const {
    if (id >= columns_.size()) {
        throw SchemaError{"column index " + std::to_string(id) + " is out of range"};
    }
    return columns_[id];
}

// Return a column by ID or name, rejecting missing columns.
const Column& Schema::column(std::string_view name) const {
    const auto id{columnId(name)};
    if (!id) {
        throw SchemaError{"unknown column: " + std::string{name}};
    }
    return column(*id);
}

// Return the number of columns in the schema.
std::size_t Schema::size() const {
    return columns_.size();
}

// Return whether the schema contains no columns.
bool Schema::empty() const {
    return columns_.empty();
}

// Return whether the schema contains the given column name.
bool Schema::hasColumn(std::string_view name) const {
    return columnId(name).has_value();
}

// Resolve a column name to its ID, or return no value if absent.
std::optional<std::size_t> Schema::columnId(std::string_view name) const {
    const auto found{nameToId_.find(std::string{name})};
    if (found == nameToId_.end()) {
        return std::nullopt;
    }
    return found->second;
}

// Check that a cell matches its column type and vector dimension.
void Schema::validateCell(std::size_t columnId, const Cell& cell) const {
    const auto& expectedColumn{column(columnId)};
    const bool correctType{(isInteger(expectedColumn.type) && std::holds_alternative<std::int64_t>(cell)) ||
        (isText(expectedColumn.type) && std::holds_alternative<std::string>(cell)) ||
        (isVector(expectedColumn.type) && std::holds_alternative<std::vector<double>>(cell))};

    if (!correctType) {
        throw SchemaError{
            "column '" + expectedColumn.name + "' expects " +
            std::string{columnTypeName(expectedColumn.type)} + " but received " +
            std::string{cellTypeName(cell)}};
    }

    if (isText(expectedColumn.type)) {
        constexpr std::string_view allowed{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"};
        if (std::get<std::string>(cell).find_first_not_of(allowed) != std::string::npos) {
            throw SchemaError{"TEXT column '" + expectedColumn.name + "' allows only ASCII letters, digits, and underscores"};
        }
    }

    if (isVector(expectedColumn.type)) {
        const auto expectedDimension{expectedColumn.vectorDimension};
        const auto receivedDimension{std::get<std::vector<double>>(cell).size()};
        if (expectedDimension != receivedDimension) {
            throw SchemaError{
                "vector column '" + expectedColumn.name + "' expects dimension " +
                std::to_string(expectedDimension) + " but received " +
                std::to_string(receivedDimension)};
        }
    }
}

// Check row width and validate each cell against the schema.
void Schema::validateRow(const Row& row) const {
    if (row.size() != size()) {
        throw SchemaError{
            "row expects " + std::to_string(size()) + " cells but received " +
            std::to_string(row.size())};
    }

    for (std::size_t index{0}; index < size(); ++index) {
        validateCell(index, row.cell(index));
    }
}

} // namespace vrdb
