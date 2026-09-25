#include "types/cell.hpp"

namespace vrdb {

// Return the active cell type name, or EMPTY for a valueless variant.
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
