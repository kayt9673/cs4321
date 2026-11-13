#pragma once

#include "vector/index.hpp"

#include <istream>
#include <ostream>

namespace vrdb {

// Stream codec only; filesystem ownership remains in FileStorageEngine.
class IndexFormat {
public:
    static void write(std::ostream& output, const VectorIndexDefinition& definition);
    static VectorIndexDefinition read(std::istream& input);
};

} // namespace vrdb
