#pragma once

#include "types/data_type.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace vrdb {

struct Column {
    std::string name;
    DataType type;
    std::size_t vectorDimension;

    // Construct a column and validate its name, type, and dimension.
    Column(std::string columnName, DataType columnType, std::size_t dimension = 0);
    // Check the column name and the dimension rules for its type.
    void validate() const;
};

} // namespace vrdb
