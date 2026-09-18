#pragma once

#include "types/value.h"

#include <cstddef>
#include <optional>
#include <string>

namespace vrdb {

struct Column {
    std::string name;
    ColumnType type;
    std::optional<std::size_t> vectorDimension;

    Column(std::string columnName, ColumnType columnType, std::optional<std::size_t> dimension = std::nullopt);
};

} // namespace vrdb
