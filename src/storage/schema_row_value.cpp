#include "types/row.h"

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace vrdb {

ColumnType valueType(const Value& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return ColumnType::INTEGER;
    }
    if (std::holds_alternative<std::string>(value)) {
        return ColumnType::TEXT;
    }
    return ColumnType::VECTOR;
}

Column::Column(std::string columnName, ColumnType columnType, std::size_t dimension)
    : name(std::move(columnName)), type(columnType), vectorDimension(dimension) {}

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns)) {
    validate();
}

const std::vector<Column>& Schema::columns() const {
    return columns_;
}

const Column& Schema::column(std::size_t index) const {
    if (index >= columns_.size()) {
        throw std::out_of_range("column index out of range");
    }
    return columns_[index];
}

std::size_t Schema::size() const {
    return columns_.size();
}

bool Schema::empty() const {
    return columns_.empty();
}

int Schema::columnIndex(const std::string& name) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Schema::validate() const {
    if (columns_.empty()) {
        throw std::invalid_argument("schema must contain at least one column");
    }

    std::unordered_set<std::string> names;
    for (const auto& column : columns_) {
        if (column.name.empty()) {
            throw std::invalid_argument("column name cannot be empty");
        }
        if (!names.insert(column.name).second) {
            throw std::invalid_argument("duplicate column name: " + column.name);
        }
        if (column.type == ColumnType::VECTOR && column.vectorDimension == 0) {
            throw std::invalid_argument("vector columns must declare a non-zero dimension");
        }
        if (column.type != ColumnType::VECTOR && column.vectorDimension != 0) {
            throw std::invalid_argument("only vector columns may declare a dimension");
        }
    }
}

Row::Row(std::vector<Value> values)
    : values_(std::move(values)) {}

const std::vector<Value>& Row::values() const {
    return values_;
}

const Value& Row::value(std::size_t index) const {
    if (index >= values_.size()) {
        throw std::out_of_range("row value index out of range");
    }
    return values_[index];
}

std::size_t Row::size() const {
    return values_.size();
}

void Row::validateAgainst(const Schema& schema) const {
    if (values_.size() != schema.size()) {
        throw std::invalid_argument("row width does not match schema");
    }

    for (std::size_t i = 0; i < values_.size(); ++i) {
        const auto& column = schema.column(i);
        if (valueType(values_[i]) != column.type) {
            throw std::invalid_argument("row value type does not match schema column: " + column.name);
        }
        if (column.type == ColumnType::VECTOR &&
            std::get<std::vector<float>>(values_[i]).size() != column.vectorDimension) {
            throw std::invalid_argument("row vector dimension does not match schema column: " + column.name);
        }
    }
}

} // namespace vrdb
