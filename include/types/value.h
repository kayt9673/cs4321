#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vrdb {

using Vector = std::vector<float>;

enum class ColumnType {
    INTEGER,
    TEXT,
    VECTOR
};

using Value = std::variant<int64_t, std::string, Vector>;

ColumnType valueType(const Value& value);

} // namespace vrdb
