#include "types/value.h"

namespace vrdb {

ColumnType valueType(const Value& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return ColumnType::INTEGER;
    }
    if (std::holds_alternative<std::string>(value)) {
        return ColumnType::TEXT;
    }
    return ColumnType::VECTOR;
}

} // namespace vrdb
