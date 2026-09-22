#include "types/value.h"

namespace vrdb {

std::string_view valueTypeName(const Value& value) noexcept {
    if (std::holds_alternative<std::int64_t>(value)) {
        return "INTEGER";
    }
    if (std::holds_alternative<std::string>(value)) {
        return "TEXT";
    }
    return "VECTOR";
}

} // namespace vrdb
