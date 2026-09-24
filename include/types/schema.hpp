#pragma once

#include "types/column.hpp"
#include "types/ids.hpp"
#include "types/cell.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vrdb {

class Row;

class Schema {
public:
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    const Column& column(ColumnId id) const;
    const Column& column(std::string_view name) const;
    std::size_t size() const;
    bool empty() const;

    bool hasColumn(std::string_view name) const;
    std::optional<ColumnId> columnId(std::string_view name) const;
    void validateCell(ColumnId column, const Cell& cell) const;
    void validateRow(const Row& row) const;

private:
    std::vector<Column> columns_;
    std::unordered_map<std::string, ColumnId> nameToId_;
};

} // namespace vrdb
