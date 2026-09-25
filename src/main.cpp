#include "db/database.hpp"
#include "query/executor.hpp"
#include "query/predicate.hpp"

#include <iostream>
#include <string>
#include <vector>

// Create sample review data and run a combined integer and vector query.
int main() {
    using namespace vrdb;

    DatabaseManager db{"./data"};

    Schema schema{{
        Column{"id", ColumnType::INTEGER},
        Column{"rating", ColumnType::INTEGER},
        Column{"review", ColumnType::TEXT},
        Column{"embedding", ColumnType::VECTOR, 4},
    }};

    if (db.hasTable("reviews")) {
        db.dropTable("reviews");
    }
    db.createTable("reviews", schema);
    db.insert("reviews", Row{{
        std::int64_t{1},
        int64_t{8},
        std::string{"Example review"},
        std::vector<double>{0.1, 0.2, 0.3, 0.4},
    }});

    Query query{};
    query.table = "reviews";
    query.predicates.push_back(integerComparison("rating", ComparisonOperator::GREATER_THAN_OR_EQUAL, 7));
    query.predicates.push_back(
        vectorDistance("embedding",
                       DistanceMetric::COSINE,
                       {0.1, 0.2, 0.3, 0.4},
                       ComparisonOperator::LESS_THAN,
                       0.01));

    const auto result{db.select(query)};

    std::cout << "Inserted rows persisted: " << db.rowCount("reviews") << '\n';
    std::cout << "Rows matching demo query: " << result.rows.size() << '\n';

    return 0;
}
