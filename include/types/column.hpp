#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace vrdb {

enum class ColumnType { INTEGER, TEXT, VECTOR };

constexpr bool isInteger(ColumnType type) noexcept { return type == ColumnType::INTEGER; }
constexpr bool isText(ColumnType type) noexcept { return type == ColumnType::TEXT; }
constexpr bool isVector(ColumnType type) noexcept { return type == ColumnType::VECTOR; }
std::string_view columnTypeName(ColumnType type);

struct Column {
    std::string name;
    ColumnType type;
    std::size_t vectorDimension;

    Column(std::string columnName, ColumnType columnType, std::size_t dimension = 0);
    void validate() const;
};

} // namespace vrdb
