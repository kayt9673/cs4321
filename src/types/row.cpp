#include "types/row.h"

#include "db/errors.h"

#include <utility>

namespace vrdb {

Row::Row(std::vector<Value> values)
    : values_(std::move(values)) {}

const std::vector<Value>& Row::values() const {
    return values_;
}

const Value& Row::value(ColumnId id) const {
    const auto index = static_cast<std::size_t>(id);
    if (index >= values_.size()) {
        throw SchemaError("ColumnId " + std::to_string(id) + " is out of range for row");
    }
    return values_[index];
}

std::size_t Row::size() const {
    return values_.size();
}

} // namespace vrdb
