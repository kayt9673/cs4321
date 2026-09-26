#pragma once

#include "types/data_type.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace vrdb {

static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::digits == 24 &&
              std::numeric_limits<float>::max_exponent == 128,
              "database vectors require IEEE 754 binary32 floats");

using Cell = std::variant<std::int64_t, std::string, std::vector<float>>;

// Return the active cell data type, or EMPTY for a valueless variant.
DataType cellTypeName(const Cell& cell) noexcept;

} // namespace vrdb
