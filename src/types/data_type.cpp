#include "types/data_type.hpp"

#include "db/errors.hpp"

namespace vrdb {

std::string_view dataTypeName(DataType type) {
    switch (type) {
    case DataType::INTEGER: return "INTEGER";
    case DataType::TEXT: return "TEXT";
    case DataType::VECTOR: return "VECTOR";
    case DataType::EMPTY: return "EMPTY";
    }
    throw SchemaError{"unknown column type"};
}

} // namespace vrdb
