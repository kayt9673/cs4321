#include "types/schema.hpp"

#include "db/errors.hpp"
#include "types/row.hpp"

#include <limits>
#include <utility>

namespace vrdb {

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns)) {
    if (columns_.empty()) {
        throw SchemaError("schema must contain at least one column");
    }

    nameToId_.reserve(columns_.size());
    for (std::size_t index = 0; index < columns_.size(); ++index) {
        if (index > std::numeric_limits<ColumnId>::max()) {
            throw SchemaError("schema has too many columns for ColumnId");
        }

        const auto& column = columns_[index];
        column.validate();
        const auto id = static_cast<ColumnId>(index);
        const auto& name = columns_[index].name;
        if (!nameToId_.emplace(name, id).second) {
            throw SchemaError("duplicate column name: " + name);
        }
    }
}

const std::vector<Column>& Schema::columns() const {
    return columns_;
}

const Column& Schema::column(ColumnId id) const {
    const auto index = static_cast<std::size_t>(id);
    if (index >= columns_.size()) {
        throw SchemaError("ColumnId " + std::to_string(id) + " is out of range");
    }
    return columns_[index];
}

const Column& Schema::column(std::string_view name) const {
    const auto id = columnId(name);
    if (!id) {
        throw SchemaError("unknown column: " + std::string(name));
    }
    return column(*id);
}

std::size_t Schema::size() const {
    return columns_.size();
}

bool Schema::empty() const {
    return columns_.empty();
}

bool Schema::hasColumn(std::string_view name) const {
    return columnId(name).has_value();
}

std::optional<ColumnId> Schema::columnId(std::string_view name) const {
    const auto found = nameToId_.find(std::string(name));
    if (found == nameToId_.end()) {
        return std::nullopt;
    }
    return found->second;
}

void Schema::validateCell(ColumnId columnId, const Cell& cell) const {
    const auto& expectedColumn = column(columnId);
    const bool correctType =
        (isInteger(expectedColumn.type) && std::holds_alternative<std::int64_t>(cell)) ||
        (isText(expectedColumn.type) && std::holds_alternative<std::string>(cell)) ||
        (isVector(expectedColumn.type) && std::holds_alternative<std::vector<double>>(cell));

    if (!correctType) {
        throw SchemaError(
            "column '" + expectedColumn.name + "' expects " +
            std::string(columnTypeName(expectedColumn.type)) + " but received " +
            std::string(cellTypeName(cell)));
    }

    if (isVector(expectedColumn.type)) {
        const auto expectedDimension = expectedColumn.vectorDimension;
        const auto receivedDimension = std::get<std::vector<double>>(cell).size();
        if (expectedDimension != receivedDimension) {
            throw SchemaError(
                "vector column '" + expectedColumn.name + "' expects dimension " +
                std::to_string(expectedDimension) + " but received " +
                std::to_string(receivedDimension));
        }
    }
}

void Schema::validateRow(const Row& row) const {
    if (row.size() != size()) {
        throw SchemaError(
            "row expects " + std::to_string(size()) + " cells but received " +
            std::to_string(row.size()));
    }

    for (std::size_t index = 0; index < size(); ++index) {
        const auto id = static_cast<ColumnId>(index);
        validateCell(id, row.cell(id));
    }
}

} // namespace vrdb
