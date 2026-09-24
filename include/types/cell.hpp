#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vrdb {

static_assert(std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53 &&
              std::numeric_limits<double>::max_exponent == 1024,
              "database vectors require IEEE 754 binary64 doubles");

using Cell = std::variant<std::int64_t, std::string, std::vector<double>>;

std::string_view cellTypeName(const Cell& cell) noexcept;

} // namespace vrdb
