#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace vrdb {

enum class ColumnType { INTEGER, TEXT, VECTOR };

// Return whether the column type is INTEGER.
constexpr bool isInteger(ColumnType type) noexcept { return type == ColumnType::INTEGER; }
// Return whether the column type is TEXT.
constexpr bool isText(ColumnType type) noexcept { return type == ColumnType::TEXT; }
// Return whether the column type is VECTOR.
constexpr bool isVector(ColumnType type) noexcept { return type == ColumnType::VECTOR; }
// Return the stored type name or reject an unknown column type.
std::string_view columnTypeName(ColumnType type);

struct Column {
    std::string name;
    ColumnType type;
    std::size_t vectorDimension;

    // Construct a column and validate its name, type, and dimension.
    Column(std::string columnName, ColumnType columnType, std::size_t dimension = 0);
    // Check the column name and the dimension rules for its type.
    void validate() const;
};

} // namespace vrdb
