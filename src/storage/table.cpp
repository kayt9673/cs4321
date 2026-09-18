#include "storage/table.h"

#include <stdexcept>
#include <utility>

namespace vrdb {

Table::Table(std::string name, Schema schema)
    : name_(std::move(name)), schema_(std::move(schema)) {}

const std::string& Table::name() const {
    return name_;
}

const Schema& Table::schema() const {
    return schema_;
}

void Table::addRow(Row row) {
    schema_.validateRow(row);
    rows_.push_back(std::move(row));
}

const std::vector<Row>& Table::rows() const {
    return rows_;
}

} // namespace vrdb
