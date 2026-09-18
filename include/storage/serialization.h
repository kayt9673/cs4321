#pragma once

#include "types/row.h"
#include "types/schema.h"
#include "types/value.h"

#include <string>

namespace vrdb {

std::string serializeValue(const Value& value);
Value deserializeValue(const std::string& encoded, const Column& column);
std::string serializeRow(const Row& row);
Row deserializeRow(const std::string& encoded, const Schema& schema);

std::string serializeColumn(const Column& column);
Column deserializeColumn(const std::string& encoded);

} // namespace vrdb
