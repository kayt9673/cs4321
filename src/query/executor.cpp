#include "query/executor.hpp"

#include "db/errors.hpp"
#include "vector/distance.hpp"

#include <type_traits>
#include <unordered_set>
#include <utility>

namespace vrdb {
namespace {

// Resolve a column name or report an unknown-column query error.
std::size_t requireColumn(const Schema& schema, std::string_view name) {
    const auto id{schema.columnId(name)};
    if (!id) {
        throw QueryError{"unknown column: " + std::string{name}};
    }
    return *id;
}

// Return the column name referenced by any predicate variant.
const std::string& predicateColumnName(const Predicate& predicate) {
    return std::visit([](const auto& typedPredicate) -> const std::string& {
        return typedPredicate.column;
    }, predicate);
}

// Check that a predicate matches its column type and vector dimension.
void validatePredicate(const Predicate& predicate, std::size_t columnId, const Schema& schema) {
    const auto& column{schema.column(columnId)};
    std::visit([&](const auto& typedPredicate) {
        using PredicateType = std::decay_t<decltype(typedPredicate)>;
        if constexpr (std::is_same_v<PredicateType, IntegerPredicate>) {
            if (!isInteger(column.type)) {
                throw QueryError{"integer predicate references a non-integer column: " + column.name};
            }
        } else if constexpr (std::is_same_v<PredicateType, TextPredicate>) {
            if (!isText(column.type)) {
                throw QueryError{"text predicate references a non-text column: " + column.name};
            }
        } else if constexpr (std::is_same_v<PredicateType, VectorPredicate>) {
            if (!isVector(column.type)) {
                throw QueryError{"vector predicate references a non-vector column: " + column.name};
            }
            const auto expectedDimension{column.vectorDimension};
            if (typedPredicate.referenceVector.size() != expectedDimension) {
                throw QueryError{
                    "vector predicate for column '" + column.name + "' expects dimension " +
                    std::to_string(expectedDimension) + " but received " +
                    std::to_string(typedPredicate.referenceVector.size())};
            }
        } else if constexpr (std::is_same_v<PredicateType, NullPredicate>) {
            if (typedPredicate.op != NullOperator::IS_NULL &&
                typedPredicate.op != NullOperator::IS_NOT_NULL) {
                throw QueryError{"unknown NULL predicate operator"};
            }
        }
    }, predicate);
}

} // namespace

QueryExecutor::Prepared QueryExecutor::prepare(const Query& query, const Schema& schema) const {
    Prepared prepared;
    std::unordered_set<std::size_t> projectedColumns;
    for (const auto& name : query.projection) {
        const auto id{requireColumn(schema, name)};
        if (!projectedColumns.insert(id).second) throw QueryError{"duplicate projected column: " + name};
        prepared.projection.push_back(id);
    }
    for (const auto& predicate : query.predicates) {
        const auto id{requireColumn(schema, predicateColumnName(predicate))};
        validatePredicate(predicate, id, schema);
        prepared.predicates.emplace_back(id, &predicate);
    }
    return prepared;
}

bool QueryExecutor::matches(const Prepared& prepared, const Row& row) const {
    for (const auto& [column, predicate] : prepared.predicates) {
        if (!evaluatePredicate(*predicate, column, row)) return false;
    }
    return true;
}

QueryExecutor::State QueryExecutor::begin(const Query& query, const Schema& schema) const {
    const auto prepared{prepare(query, schema)};
    return State{QueryResult{projectSchema(schema, prepared.projection), {}, {}}, 0,
                 query.limit && *query.limit == 0};
}

void QueryExecutor::executeBatch(const Query& query, const Schema& schema,
                                 const std::vector<StoredRow>& rows, State& state) const {
    if (state.complete) return;
    const auto prepared{prepare(query, schema)};
    for (const auto& stored : rows) {
        if (!matches(prepared, stored.row)) continue;
        if (state.skipped < query.offset) {
            ++state.skipped;
            continue;
        }
        state.result.rows.push_back(projectRow(stored.row, prepared.projection));
        state.result.rowIds.push_back(stored.id);
        if (query.limit && state.result.rows.size() >= *query.limit) {
            state.complete = true;
            break;
        }
    }
}

// Filter rows, then apply offset, projection, and limit in input order.
QueryResult QueryExecutor::execute(const Query& query, const Schema& schema,
                                   const std::vector<StoredRow>& rows) const {
    auto state{begin(query, schema)};
    executeBatch(query, schema, rows, state);
    return std::move(state.result);
}

// Evaluate one typed predicate against a row cell.
bool QueryExecutor::evaluatePredicate(const Predicate& predicate, std::size_t column, const Row& row) const {
    const auto& cell{row.cell(column)};
    if (cell.valueless_by_exception()) throw QueryError{"valueless row cell"};
    if (isNullCell(cell) && !std::holds_alternative<NullPredicate>(predicate)) return false;
    return std::visit([&](const auto& typedPredicate) -> bool {
        using PredicateType = std::decay_t<decltype(typedPredicate)>;
        if constexpr (std::is_same_v<PredicateType, NullPredicate>) {
            if (typedPredicate.op == NullOperator::IS_NULL) return isNullCell(cell);
            if (typedPredicate.op == NullOperator::IS_NOT_NULL) return !isNullCell(cell);
            throw QueryError{"unknown NULL predicate operator"};
        } else if constexpr (std::is_same_v<PredicateType, IntegerPredicate>) {
            const auto* value{std::get_if<std::int64_t>(&row.cell(column))};
            if (!value) {
                throw QueryError{"row cell is not an integer"};
            }
            return evaluateIntegerComparison(*value, typedPredicate.op, typedPredicate.value);
        } else if constexpr (std::is_same_v<PredicateType, TextPredicate>) {
            const auto* value{std::get_if<std::string>(&row.cell(column))};
            if (!value) {
                throw QueryError{"row cell is not text"};
            }
            return evaluateTextComparison(*value, typedPredicate.op, typedPredicate.value);
        } else if constexpr (std::is_same_v<PredicateType, VectorPredicate>) {
            const auto* value{std::get_if<std::vector<float>>(&row.cell(column))};
            if (!value) {
                throw QueryError{"row cell is not a vector"};
            }
            const auto actualDistance{distance(*value, typedPredicate.referenceVector, typedPredicate.metric)};
            return evaluateFloatComparison(actualDistance, typedPredicate.op, typedPredicate.threshold);
        }
    }, predicate);
}

// Copy the selected cells, or the whole row for an empty projection.
Row QueryExecutor::projectRow(const Row& row, const std::vector<std::size_t>& projection) const {
    if (projection.empty()) {
        return row;
    }

    std::vector<Cell> cells{};
    cells.reserve(projection.size());
    for (const auto column : projection) {
        cells.push_back(row.cell(column));
    }
    return Row{std::move(cells)};
}

// Build the selected-column schema, or copy it for an empty projection.
Schema QueryExecutor::projectSchema(const Schema& schema, const std::vector<std::size_t>& projection) const {
    if (projection.empty()) {
        return schema;
    }

    std::vector<Column> columns{};
    columns.reserve(projection.size());
    for (const auto column : projection) {
        columns.push_back(schema.column(column));
    }
    return Schema{std::move(columns)};
}

} // namespace vrdb
