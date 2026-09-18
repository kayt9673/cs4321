#pragma once

#include "types/data_type.h"
#include "types/ids.h"
#include "types/value.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vrdb {

class Row;

struct Column {
    std::string name;
    DataType type;

    Column(std::string columnName, DataType dataType);
};

class Schema {
public:
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    const Column& column(ColumnId id) const;
    std::size_t size() const;
    bool empty() const;

    std::optional<ColumnId> columnId(std::string_view name) const;
    void validateValue(ColumnId column, const Value& value) const;
    void validateRow(const Row& row) const;

private:
    std::vector<Column> columns_;
    std::unordered_map<std::string, ColumnId> nameToId_;
};

} // namespace vrdb
