#include "types/row.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

// Take ownership of the supplied cells in their existing order.
Row::Row(std::vector<Cell> cells)
    : cells_{std::move(cells)} {}

// Return a read-only view of all cells in the row.
const std::vector<Cell>& Row::cells() const {
    return cells_;
}

// Return a cell by column ID, rejecting out-of-range IDs.
const Cell& Row::cell(std::size_t id) const {
    if (id >= cells_.size()) {
        throw SchemaError{"column index " + std::to_string(id) + " is out of range for row"};
    }
    return cells_[id];
}

// Return the number of cells in the row.
std::size_t Row::size() const {
    return cells_.size();
}

} // namespace vrdb
