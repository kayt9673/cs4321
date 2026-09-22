#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vrdb {

using VectorValue = std::vector<float>;
using Vector = VectorValue;

using Value = std::variant<std::int64_t, std::string, VectorValue>;

std::string_view valueTypeName(const Value& value) noexcept;

} // namespace vrdb
