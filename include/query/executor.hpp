#pragma once

#include "query/query.hpp"
#include "query/query_result.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"

#include <vector>

namespace vrdb {

class QueryExecutor {
public:
    // Filter rows, then apply offset, projection, and limit in input order.
    QueryResult execute(const Query& query, const Schema& schema, const std::vector<Row>& rows) const;

private:
    // Evaluate one typed predicate against a row cell.
    bool evaluatePredicate(const Predicate& predicate, ColumnId column, const Row& row) const;
    // Copy the selected cells, or the whole row for an empty projection.
    Row projectRow(const Row& row, const std::vector<ColumnId>& projection) const;
    // Build the selected-column schema, or copy it for an empty projection.
    Schema projectSchema(const Schema& schema, const std::vector<ColumnId>& projection) const;
};

} // namespace vrdb
