#pragma once

#include "types/row.h"
#include "types/schema.h"

#include <string>
#include <vector>

namespace vrdb {

class Table {
public:
    Table(std::string name, Schema schema);

    const std::string& name() const;
    const Schema& schema() const;

    void addRow(Row row);
    const std::vector<Row>& rows() const;

private:
    std::string name_;
    Schema schema_;
    std::vector<Row> rows_;
};

} // namespace vrdb
