#include "types/row.h"

#include "db/errors.h"

#include <utility>

namespace vrdb {

Row::Row(std::vector<Value> values)
    : values_(std::move(values)) {}

const std::vector<Value>& Row::values() const {
    return values_;
}

const Value& Row::value(std::size_t index) const {
    if (index >= values_.size()) {
        throw SchemaError("row value index out of range");
    }
    return values_[index];
}

std::size_t Row::size() const {
    return values_.size();
}

void Row::validateAgainst(const Schema& schema) const {
    if (values_.size() != schema.size()) {
        throw SchemaError("row width does not match schema");
    }

    for (std::size_t i = 0; i < values_.size(); ++i) {
        const auto& column = schema.column(i);
        if (valueType(values_[i]) != column.type) {
            throw SchemaError("row value type does not match schema column: " + column.name);
        }
        if (column.type == ColumnType::VECTOR && column.vectorDimension &&
            std::get<Vector>(values_[i]).size() != *column.vectorDimension) {
            throw SchemaError("row vector dimension does not match schema column: " + column.name);
        }
    }
}

} // namespace vrdb
