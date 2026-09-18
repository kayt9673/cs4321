#pragma once

#include "types/column.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vrdb {

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);

    const std::vector<Column>& columns() const;
    const Column& column(std::size_t index) const;
    const Column& column(const std::string& name) const;
    std::size_t size() const;
    bool empty() const;

    bool hasColumn(const std::string& name) const;
    std::size_t columnIndex(const std::string& name) const;
    void validate() const;

private:
    std::vector<Column> columns_;
};

} // namespace vrdb
