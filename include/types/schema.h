#pragma once

#include "types/value.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vrdb {

struct Column {
    std::string name;
    ColumnType type;
    std::size_t vectorDimension;

    Column(std::string columnName, ColumnType columnType, std::size_t dimension = 0);
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    const Column& column(std::size_t index) const;
    std::size_t size() const;
    bool empty() const;

    int columnIndex(const std::string& name) const;
    void validate() const;

private:
    std::vector<Column> columns_;
};

} // namespace vrdb
