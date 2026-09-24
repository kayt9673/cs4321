#pragma once

#include "query/predicate.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vrdb {

struct Query {
    std::string table;
    std::vector<std::string> projection;
    std::vector<Predicate> predicates;
    std::optional<std::size_t> limit;
    std::size_t offset = 0;
};

} // namespace vrdb
