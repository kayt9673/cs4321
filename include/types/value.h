#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vrdb {

enum class ColumnType {
    INTEGER,
    TEXT,
    VECTOR
};

using Value = std::variant<int64_t, std::string, std::vector<float>>;

ColumnType valueType(const Value& value);

} // namespace vrdb
