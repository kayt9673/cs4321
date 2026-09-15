#pragma once

#include "types/schema.h"
#include "types/value.h"

#include <vector>

namespace vrdb {

class Row {
public:
    Row() = default;
    explicit Row(std::vector<Value> values);

    const std::vector<Value>& values() const;
    const Value& value(std::size_t index) const;
    std::size_t size() const;

    void validateAgainst(const Schema& schema) const;

private:
    std::vector<Value> values_;
};

} // namespace vrdb
