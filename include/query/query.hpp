#pragma once

#include "query/predicate.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vrdb {

struct Query {
    std::string table;
    std::vector<std::string> projection;
    std::vector<Predicate> predicates;
    std::optional<std::size_t> limit;
    std::size_t offset{0};
};

struct QueryResult {
    Schema schema;
    std::vector<Row> rows;
};

} // namespace vrdb
