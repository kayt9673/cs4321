#include "types/schema.h"

#include "db/errors.h"

#include <unordered_set>
#include <utility>

namespace vrdb {

Column::Column(std::string columnName, ColumnType columnType, std::optional<std::size_t> dimension)
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
        throw SchemaError("column index out of range");
    }
    return columns_[index];
}

const Column& Schema::column(const std::string& name) const {
    return column(columnIndex(name));
}

std::size_t Schema::size() const {
    return columns_.size();
}

bool Schema::empty() const {
    return columns_.empty();
}

bool Schema::hasColumn(const std::string& name) const {
    for (const auto& column : columns_) {
        if (column.name == name) {
            return true;
        }
    }
    return false;
}

std::size_t Schema::columnIndex(const std::string& name) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return i;
        }
    }
    throw SchemaError("unknown column: " + name);
}

void Schema::validate() const {
    if (columns_.empty()) {
        throw SchemaError("schema must contain at least one column");
    }

    std::unordered_set<std::string> names;
    for (const auto& column : columns_) {
        if (column.name.empty()) {
            throw SchemaError("column name cannot be empty");
        }
        if (!names.insert(column.name).second) {
            throw SchemaError("duplicate column name: " + column.name);
        }
        if (column.type == ColumnType::VECTOR && !column.vectorDimension) {
            throw SchemaError("vector columns must declare a dimension: " + column.name);
        }
        if (column.type == ColumnType::VECTOR && *column.vectorDimension == 0) {
            throw SchemaError("vector columns must declare a non-zero dimension");
        }
        if (column.type != ColumnType::VECTOR && column.vectorDimension) {
            throw SchemaError("only vector columns may declare a dimension");
        }
    }
}

} // namespace vrdb
