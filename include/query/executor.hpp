#pragma once

#include "query/query.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"

#include <cstddef>
#include <vector>
#include <utility>

namespace vrdb {

class QueryExecutor {
public:
    struct Prepared {
        std::vector<std::size_t> projection;
        // Borrows predicates from the Query; keep it alive while matching rows.
        std::vector<std::pair<std::size_t, const Predicate*>> predicates;
    };
    Prepared prepare(const Query& query, const Schema& schema) const;
    bool matches(const Prepared& prepared, const Row& row) const;
    Row projectRow(const Row& row, const std::vector<std::size_t>& projection) const;
    Schema projectSchema(const Schema& schema, const std::vector<std::size_t>& projection) const;
    struct State {
        QueryResult result;
        std::size_t skipped{0};
        bool complete{false};
    };

    // Initialize a result that can be filled by one or more input batches.
    State begin(const Query& query, const Schema& schema) const;
    // Process one batch, preserving offset and limit state across calls.
    void executeBatch(const Query& query, const Schema& schema,
                      const std::vector<StoredRow>& rows, State& state) const;
    // Filter rows, then apply offset, projection, and limit in input order.
    QueryResult execute(const Query& query, const Schema& schema, const std::vector<StoredRow>& rows) const;

private:
    // Evaluate one typed predicate against a row cell.
    bool evaluatePredicate(const Predicate& predicate, std::size_t column, const Row& row) const;
};

} // namespace vrdb
