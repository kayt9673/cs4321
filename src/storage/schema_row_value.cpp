#include "types/row.h"
#include "types/schema.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace vrdb {

VectorType::VectorType(std::size_t dimension)
    : dimension_(dimension) {
    if (dimension_ == 0) {
        throw std::invalid_argument("Vector dimension must be greater than zero");
    }
}

std::size_t VectorType::dimension() const noexcept {
    return dimension_;
}

bool isInteger(const DataType& type) noexcept {
    return std::holds_alternative<Int64Type>(type);
}

bool isText(const DataType& type) noexcept {
    return std::holds_alternative<TextType>(type);
}

bool isVector(const DataType& type) noexcept {
    return std::holds_alternative<VectorType>(type);
}

std::string_view dataTypeName(const DataType& type) noexcept {
    if (isInteger(type)) {
        return "INTEGER";
    }
    if (isText(type)) {
        return "TEXT";
    }
    return "VECTOR";
}

std::string_view valueTypeName(const Value& value) noexcept {
    if (std::holds_alternative<std::int64_t>(value)) {
        return "INTEGER";
    }
    if (std::holds_alternative<std::string>(value)) {
        return "TEXT";
    }
    return "VECTOR";
}

Column::Column(std::string columnName, DataType dataType)
    : name(std::move(columnName)), type(std::move(dataType)) {
    if (name.empty()) {
        throw std::invalid_argument("Column name cannot be empty");
    }
}

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns)) {
    if (columns_.empty()) {
        throw std::invalid_argument("Schema must contain at least one column");
    }

    nameToId_.reserve(columns_.size());
    for (std::size_t index = 0; index < columns_.size(); ++index) {
        if (index > std::numeric_limits<ColumnId>::max()) {
            throw std::invalid_argument("Schema has too many columns for ColumnId");
        }

        const auto id = static_cast<ColumnId>(index);
        const auto& name = columns_[index].name;
        if (name.empty()) {
            throw std::invalid_argument("Column name cannot be empty");
        }
        if (!nameToId_.emplace(name, id).second) {
            throw std::invalid_argument("Duplicate column name: " + name);
        }
    }
}

const std::vector<Column>& Schema::columns() const {
    return columns_;
}

const Column& Schema::column(ColumnId id) const {
    const auto index = static_cast<std::size_t>(id);
    if (index >= columns_.size()) {
        throw std::out_of_range("ColumnId " + std::to_string(id) + " is out of range");
    }
    return columns_[index];
}

std::size_t Schema::size() const {
    return columns_.size();
}

bool Schema::empty() const {
    return columns_.empty();
}

std::optional<ColumnId> Schema::columnId(std::string_view name) const {
    const auto found = nameToId_.find(std::string(name));
    if (found == nameToId_.end()) {
        return std::nullopt;
    }
    return found->second;
}

void Schema::validateValue(ColumnId columnId, const Value& value) const {
    const auto& expectedColumn = column(columnId);

    const bool correctType =
        (isInteger(expectedColumn.type) && std::holds_alternative<std::int64_t>(value)) ||
        (isText(expectedColumn.type) && std::holds_alternative<std::string>(value)) ||
        (isVector(expectedColumn.type) && std::holds_alternative<VectorValue>(value));

    if (!correctType) {
        throw std::invalid_argument(
            "Column '" + expectedColumn.name + "' expects " +
            std::string(dataTypeName(expectedColumn.type)) + " but received " +
            std::string(valueTypeName(value)));
    }

    if (isVector(expectedColumn.type)) {
        const auto expectedDimension = std::get<VectorType>(expectedColumn.type).dimension();
        const auto receivedDimension = std::get<VectorValue>(value).size();
        if (expectedDimension != receivedDimension) {
            throw std::invalid_argument(
                "Vector column '" + expectedColumn.name + "' expects dimension " +
                std::to_string(expectedDimension) + " but received " +
                std::to_string(receivedDimension));
        }
    }
}

void Schema::validateRow(const Row& row) const {
    if (row.size() != size()) {
        throw std::invalid_argument(
            "Row expects " + std::to_string(size()) + " values but received " +
            std::to_string(row.size()));
    }

    for (std::size_t index = 0; index < size(); ++index) {
        const auto id = static_cast<ColumnId>(index);
        validateValue(id, row.value(id));
    }
}

Row::Row(std::vector<Value> values)
    : values_(std::move(values)) {}

const std::vector<Value>& Row::values() const {
    return values_;
}

const Value& Row::value(ColumnId id) const {
    const auto index = static_cast<std::size_t>(id);
    if (index >= values_.size()) {
        throw std::out_of_range("ColumnId " + std::to_string(id) + " is out of range for row");
    }
    return values_[index];
}

std::size_t Row::size() const {
    return values_.size();
}

} // namespace vrdb
