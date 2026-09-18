#include "query/executor.h"

#include "db/errors.h"
#include "vector/distance.h"

#include <type_traits>
#include <utility>

namespace vrdb {

QueryResult QueryExecutor::execute(const Query& query, const Schema& schema, const std::vector<Row>& rows) const {
    QueryResult result{projectSchema(schema, query.projection), {}};
    std::size_t skipped = 0;

    for (const auto& row : rows) {
        bool include = true;
        for (const auto& predicate : query.predicates) {
            if (!evaluatePredicate(predicate, schema, row)) {
                include = false;
                break;
            }
        }
        if (include) {
            if (skipped < query.offset) {
                ++skipped;
                continue;
            }
            result.rows.push_back(projectRow(row, schema, query.projection));
            if (query.limit && result.rows.size() >= *query.limit) {
                break;
            }
        }
    }
    return result;
}

bool QueryExecutor::evaluatePredicate(const Predicate& predicate, const Schema& schema, const Row& row) const {
    return std::visit([&](const auto& typedPredicate) -> bool {
        using PredicateType = std::decay_t<decltype(typedPredicate)>;
        const auto index = schema.columnIndex(typedPredicate.column);
        const auto& column = schema.column(index);

        if constexpr (std::is_same_v<PredicateType, IntegerPredicate>) {
            if (column.type != ColumnType::INTEGER) {
                throw QueryError("integer predicate references a non-integer column");
            }
            const auto* value = std::get_if<int64_t>(&row.value(index));
            if (!value) {
                throw QueryError("row value is not an integer");
            }
            return evaluateIntegerComparison(*value, typedPredicate.op, typedPredicate.value);
        } else if constexpr (std::is_same_v<PredicateType, TextPredicate>) {
            if (column.type != ColumnType::TEXT) {
                throw QueryError("text predicate references a non-text column");
            }
            const auto* value = std::get_if<std::string>(&row.value(index));
            if (!value) {
                throw QueryError("row value is not text");
            }
            return evaluateTextComparison(*value, typedPredicate.op, typedPredicate.value);
        } else {
            if (column.type != ColumnType::VECTOR) {
                throw QueryError("vector predicate references a non-vector column");
            }
            if (!column.vectorDimension || typedPredicate.referenceVector.size() != *column.vectorDimension) {
                throw QueryError("vector predicate reference dimension does not match column");
            }
            const auto* value = std::get_if<Vector>(&row.value(index));
            if (!value) {
                throw QueryError("row value is not a vector");
            }
            const auto actualDistance = distance(*value, typedPredicate.referenceVector, typedPredicate.metric);
            return evaluateFloatComparison(actualDistance, typedPredicate.op, typedPredicate.threshold);
        }
    }, predicate);
}

Row QueryExecutor::projectRow(const Row& row, const Schema& schema, const std::vector<std::string>& projection) const {
    if (projection.empty()) {
        return row;
    }

    std::vector<Value> values;
    values.reserve(projection.size());
    for (const auto& columnName : projection) {
        values.push_back(row.value(schema.columnIndex(columnName)));
    }
    return Row(std::move(values));
}

Schema QueryExecutor::projectSchema(const Schema& schema, const std::vector<std::string>& projection) const {
    if (projection.empty()) {
        return schema;
    }

    std::vector<Column> columns;
    columns.reserve(projection.size());
    for (const auto& columnName : projection) {
        columns.push_back(schema.column(columnName));
    }
    return Schema(std::move(columns));
}

} // namespace vrdb
