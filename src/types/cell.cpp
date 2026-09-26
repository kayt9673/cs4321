#include "types/cell.hpp"

namespace vrdb {

// Return the active cell data type, or EMPTY for a valueless variant.
DataType cellTypeName(const Cell& cell) noexcept {
    if (std::holds_alternative<std::int64_t>(cell)) {
        return DataType::INTEGER;
    }
    if (std::holds_alternative<std::string>(cell)) {
        return DataType::TEXT;
    }
    if (std::holds_alternative<std::vector<double>>(cell)) {
        return DataType::VECTOR;
    }
    return DataType::EMPTY;
}

} // namespace vrdb
