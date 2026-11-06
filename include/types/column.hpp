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
    bool nullable;

    // Newly constructed columns allow NULL unless explicitly disabled.
    // Persisted legacy catalogs override this default when loading columns.
    Column(std::string columnName, DataType columnType, std::size_t dimension = 0,
           bool allowsNull = true);
    // Check the column name and the dimension rules for its type.
    void validate() const;
};

} // namespace vrdb
