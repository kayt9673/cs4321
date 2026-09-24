#pragma once

#include "types/row.hpp"
#include "types/schema.hpp"

#include <vector>

namespace vrdb {

struct QueryResult {
    Schema schema;
    std::vector<Row> rows;
};

} // namespace vrdb
