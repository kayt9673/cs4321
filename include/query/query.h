#pragma once

#include "query/predicate.h"

#include <string>
#include <vector>

namespace vrdb {

struct Query {
    std::string table;
    std::vector<Predicate> predicates;
};

} // namespace vrdb
