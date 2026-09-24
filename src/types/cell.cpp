#include "types/cell.hpp"

namespace vrdb {

std::string_view cellTypeName(const Cell& cell) noexcept {
    if (std::holds_alternative<std::int64_t>(cell)) {
        return "INTEGER";
    }
    if (std::holds_alternative<std::string>(cell)) {
        return "TEXT";
    }
    if (std::holds_alternative<std::vector<double>>(cell)) {
        return "VECTOR";
    }
    return "EMPTY";
}

} // namespace vrdb
