#pragma once

#include "query/query.h"
#include "storage/table.h"

#include <vector>

namespace vrdb {

class QueryExecutor {
public:
    std::vector<Row> execute(const Query& query, const Table& table) const;

private:
    bool matches(const Row& row, const Table& table, const Predicate& predicate) const;
};

} // namespace vrdb
