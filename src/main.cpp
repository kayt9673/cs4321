#include "db/database_manager.hpp"
#include "storage/serialization.hpp"

#include <iostream>
#include <string>

// Query the benchmark for the first generated title and print matching IDs and titles.
int main() {
    using namespace vrdb;

    DatabaseManager db{"data/benchmark"};
    Query query{};
    query.table = "documents";
    query.projection = {"id", "title"};

    std::string title{"document-000000000001"};
    title.resize(128, 'x'); // Generated titles are padded to 128 characters.
    query.predicates.push_back(
        textComparison("title", ComparisonOperator::EQUAL, title));

    const auto result{db.select(query)};
    writeCsvRecord(std::cout, {"__vrdb_null_bitmap", "id", "title"});
    for (const auto& row : result.rows) {
        auto fields{serializeRowForCsv(row)};
        fields.insert(fields.begin(), serializeNullBitmap(row));
        writeCsvRecord(std::cout, fields);
    }
}
