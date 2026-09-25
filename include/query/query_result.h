#pragma once

#include "types/row.h"
#include "types/schema.h"

#include <vector>

namespace vrdb {

struct QueryResult {
    Schema schema;
    std::vector<Row> rows;
    std::vector<RowId> rowIds;
};

} // namespace vrdb
