#pragma once

#include <string_view>

namespace vrdb {

enum class DataType { INTEGER, TEXT, VECTOR, EMPTY };

constexpr bool isInteger(DataType type) noexcept { return type == DataType::INTEGER; }
constexpr bool isText(DataType type) noexcept { return type == DataType::TEXT; }
constexpr bool isVector(DataType type) noexcept { return type == DataType::VECTOR; }

// Return a data type's name for display and persistence.
std::string_view dataTypeName(DataType type);

} // namespace vrdb
