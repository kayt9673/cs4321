#pragma once

#include "query/query.h"
#include "query/query_result.h"
#include "types/row.h"
#include "types/schema.h"

#include <vector>

namespace vrdb {

class QueryExecutor {
public:
    QueryResult execute(const Query& query, const Schema& schema, const std::vector<Row>& rows) const;

private:
    bool evaluatePredicate(const Predicate& predicate, const Schema& schema, const Row& row) const;
    Row projectRow(const Row& row, const Schema& schema, const std::vector<std::string>& projection) const;
    Schema projectSchema(const Schema& schema, const std::vector<std::string>& projection) const;
};

} // namespace vrdb
