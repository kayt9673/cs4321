#include "db/database.h"
#include "query/executor.h"
#include "query/predicate.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace vrdb;

    Database db("./data");

    Schema schema({
        Column("id", Int64Type{}),
        Column("rating", Int64Type{}),
        Column("review", TextType{}),
        Column("embedding", VectorType{4}),
    });

    db.createTable("reviews", schema);
    db.insert("reviews", {
        int64_t{1},
        int64_t{8},
        std::string{"Example review"},
        std::vector<float>{0.1f, 0.2f, 0.3f, 0.4f},
    });

    Query query;
    query.table = "reviews";
    query.predicates.push_back(
        Predicate::integerComparison("rating", ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));
    query.predicates.push_back(
        Predicate::vectorDistanceLessThan("embedding", {0.1f, 0.2f, 0.3f, 0.4f}, 0.01f));

    QueryExecutor executor;
    const auto results = executor.execute(query, db.table("reviews"));

    std::cout << "Inserted rows persisted: " << db.rows("reviews").size() << '\n';
    std::cout << "Rows matching demo query: " << results.size() << '\n';

    return 0;
}
