#include "types/column.hpp"

#include "db/errors.hpp"

#include <utility>

namespace vrdb {

// Construct a column and validate its name, type, and dimension.
Column::Column(std::string columnName, DataType columnType, std::size_t dimension)
    : name{std::move(columnName)}, type{columnType}, vectorDimension{dimension} {
    validate();
}

// Check the column name and the dimension rules for its type.
void Column::validate() const {
    dataTypeName(type);
    if (type == DataType::EMPTY) {
        throw SchemaError{"unknown column type"};
    }
    if (isVector(type) && vectorDimension == 0) {
        throw SchemaError{"vector dimension must be greater than zero"};
    }
    if (!isVector(type) && vectorDimension != 0) {
        throw SchemaError{"only vector columns may declare a dimension"};
    }
    if (name.empty()) {
        throw SchemaError{"column name cannot be empty"};
    }
    constexpr std::string_view allowed{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"};
    if (name.find_first_not_of(allowed) != std::string::npos) {
        throw SchemaError{"column name allows only ASCII letters, digits, and underscores"};
    }
}

} // namespace vrdb
