#include "types/data_type.h"

#include "db/errors.h"

namespace vrdb {

VectorType::VectorType(std::size_t dimension)
    : dimension_(dimension) {
    if (dimension_ == 0) {
        throw SchemaError("vector dimension must be greater than zero");
    }
}

std::size_t VectorType::dimension() const noexcept {
    return dimension_;
}

bool isInteger(const DataType& type) noexcept {
    return std::holds_alternative<Int64Type>(type);
}

bool isText(const DataType& type) noexcept {
    return std::holds_alternative<TextType>(type);
}

bool isVector(const DataType& type) noexcept {
    return std::holds_alternative<VectorType>(type);
}

std::string_view dataTypeName(const DataType& type) noexcept {
    if (isInteger(type)) {
        return "INTEGER";
    }
    if (isText(type)) {
        return "TEXT";
    }
    return "VECTOR";
}

} // namespace vrdb
