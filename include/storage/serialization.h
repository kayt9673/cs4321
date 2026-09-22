#pragma once

#include "types/data_type.h"
#include "types/row.h"
#include "types/schema.h"
#include "types/value.h"

#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace vrdb {

void writeCsvRecord(std::ostream& output, const std::vector<std::string>& fields);
bool readCsvRecord(std::istream& input, std::vector<std::string>& fields);

std::string serializeValueForCsv(const Value& value);
Value deserializeValueFromCsv(const std::string& field, const Column& column);
std::vector<std::string> serializeRowForCsv(const Row& row);
Row deserializeRowFromCsv(const std::vector<std::string>& fields, const Schema& schema);

std::string serializeTypeDimension(const DataType& type);
DataType deserializeDataType(const std::string& name, const std::string& dimension);

} // namespace vrdb
