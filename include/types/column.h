#pragma once

#include "types/data_type.h"

#include <string>

namespace vrdb {

struct Column {
    std::string name;
    DataType type;

    Column(std::string columnName, DataType dataType);
};

} // namespace vrdb
