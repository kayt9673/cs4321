#pragma once

#include "query/query.hpp"
#include "query/query_result.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"

#include <vector>

namespace vrdb {

class QueryExecutor {
public:
    QueryResult execute(const Query& query, const Schema& schema, const std::vector<Row>& rows) const;

private:
    bool evaluatePredicate(const Predicate& predicate, ColumnId column, const Row& row) const;
    Row projectRow(const Row& row, const std::vector<ColumnId>& projection) const;
    Schema projectSchema(const Schema& schema, const std::vector<ColumnId>& projection) const;
};

} // namespace vrdb
