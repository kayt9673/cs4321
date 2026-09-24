#pragma once

#include "types/column.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"
#include "types/cell.hpp"

#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace vrdb {

void writeCsvRecord(std::ostream& output, const std::vector<std::string>& fields);
bool readCsvRecord(std::istream& input, std::vector<std::string>& fields);

std::string serializeCellForCsv(const Cell& cell);
Cell deserializeCellFromCsv(const std::string& field, const Column& column);
std::vector<std::string> serializeRowForCsv(const Row& row);
Row deserializeRowFromCsv(const std::vector<std::string>& fields, const Schema& schema);

std::string serializeTypeDimension(const Column& column);
Column deserializeColumn(const std::string& columnName, const std::string& typeName, const std::string& dimension);

} // namespace vrdb
