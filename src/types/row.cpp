#include "types/row.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

Row::Row(std::vector<Cell> cells)
    : cells_(std::move(cells)) {}

const std::vector<Cell>& Row::cells() const {
    return cells_;
}

const Cell& Row::cell(ColumnId id) const {
    const auto index = static_cast<std::size_t>(id);
    if (index >= cells_.size()) {
        throw SchemaError("ColumnId " + std::to_string(id) + " is out of range for row");
    }
    return cells_[index];
}

std::size_t Row::size() const {
    return cells_.size();
}

} // namespace vrdb
