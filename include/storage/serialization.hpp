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

// Write one CSV record with quoted fields and escaped quotes.
void writeCsvRecord(std::ostream& output, const std::vector<std::string>& fields);
// Read a record written by writeCsvRecord; return false only at initial EOF.
bool readCsvRecord(std::istream& input, std::vector<std::string>& fields);

// Encode a cell as text, preserving double precision in vectors.
std::string serializeCellForCsv(const Cell& cell);
// Decode a CSV field according to its column definition.
Cell deserializeCellFromCsv(const std::string& field, const Column& column);
// Encode each row cell as a CSV field.
std::vector<std::string> serializeRowForCsv(const Row& row);
// Check field count and decode a row using its schema.
Row deserializeRowFromCsv(const std::vector<std::string>& fields, const Schema& schema);

// Return the vector dimension as text, or an empty string for other types.
std::string serializeTypeDimension(const Column& column);
// Reconstruct and validate a column from catalog fields.
Column deserializeColumn(const std::string& columnName, const std::string& typeName, const std::string& dimension);

} // namespace vrdb
