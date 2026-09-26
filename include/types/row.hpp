#pragma once

#include "types/cell.hpp"

#include <cstddef>
#include <vector>

namespace vrdb {

class Row {
public:
    // Take ownership of the supplied cells in their existing order.
    explicit Row(std::vector<Cell> cells);

    // Return a read-only view of all cells in the row.
    const std::vector<Cell>& cells() const;
    // Return a cell by column ID, rejecting out-of-range IDs.
    const Cell& cell(std::size_t id) const;
    // Return the number of cells in the row.
    std::size_t size() const;

private:
    std::vector<Cell> cells_;
};


} // namespace vrdb
