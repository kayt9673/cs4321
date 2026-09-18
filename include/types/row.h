#pragma once

#include "types/ids.h"
#include "types/value.h"

#include <cstddef>
#include <vector>

namespace vrdb {

class Row {
public:
    explicit Row(std::vector<Value> values);

    const std::vector<Value>& values() const;
    const Value& value(ColumnId id) const;
    std::size_t size() const;

private:
    std::vector<Value> values_;
};

struct StoredRow {
    RowId id;
    Row row;
};

} // namespace vrdb
