#pragma once

#include "types/ids.hpp"
#include "types/cell.hpp"

#include <cstddef>
#include <vector>

namespace vrdb {

class Row {
public:
    explicit Row(std::vector<Cell> cells);

    const std::vector<Cell>& cells() const;
    const Cell& cell(ColumnId id) const;
    std::size_t size() const;

private:
    std::vector<Cell> cells_;
};

struct StoredRow {
    RowId id;
    Row row;
};

} // namespace vrdb
