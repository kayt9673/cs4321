#include "types/column.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

// Return the stored type name or reject an unknown column type.
std::string_view columnTypeName(ColumnType type) {
    switch (type) {
    case ColumnType::INTEGER: return "INTEGER";
    case ColumnType::TEXT: return "TEXT";
    case ColumnType::VECTOR: return "VECTOR";
    }
    throw SchemaError{"unknown column type"};
}

// Construct a column and validate its name, type, and dimension.
Column::Column(std::string columnName, ColumnType columnType, std::size_t dimension)
    : name{std::move(columnName)}, type{columnType}, vectorDimension{dimension} {
    validate();
}

// Check the column name and the dimension rules for its type.
void Column::validate() const {
    columnTypeName(type);
    if (isVector(type) && vectorDimension == 0) {
        throw SchemaError{"vector dimension must be greater than zero"};
    }
    if (!isVector(type) && vectorDimension != 0) {
        throw SchemaError{"only vector columns may declare a dimension"};
    }
    if (name.empty()) {
        throw SchemaError{"column name cannot be empty"};
    }
}

} // namespace vrdb
