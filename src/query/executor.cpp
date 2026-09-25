#include "query/executor.hpp"

#include "db/errors.hpp"
#include "vector/distance.hpp"

#include <type_traits>
#include <unordered_set>
#include <utility>

namespace vrdb {
namespace {

struct ResolvedPredicate {
    ColumnId column;
    const Predicate* predicate;
};

// Resolve a column name or report an unknown-column query error.
ColumnId requireColumn(const Schema& schema, std::string_view name) {
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
void validatePredicate(const Predicate& predicate, ColumnId columnId, const Schema& schema) {
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
        } else {
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
        }
    }, predicate);
}

} // namespace

// Filter rows, then apply offset, projection, and limit in input order.
QueryResult QueryExecutor::execute(const Query& query, const Schema& schema, const std::vector<Row>& rows) const {
    std::vector<ColumnId> projection{};
    projection.reserve(query.projection.size());
    std::unordered_set<ColumnId> projectedColumns{};

    for (const auto& columnName : query.projection) {
        // think we are doing double checks here but this is okay for now
        const auto id{requireColumn(schema, columnName)};
        if (!projectedColumns.insert(id).second) {
            throw QueryError{"duplicate projected column: " + columnName};
        }
        projection.push_back(id);
    }

    std::vector<ResolvedPredicate> predicates{};
    predicates.reserve(query.predicates.size());
    for (const auto& predicate : query.predicates) {
        const auto id{requireColumn(schema, predicateColumnName(predicate))};
        validatePredicate(predicate, id, schema);
        predicates.push_back(ResolvedPredicate{id, &predicate});
    }

    QueryResult result{projectSchema(schema, projection), {}};
    if (query.limit && *query.limit == 0) {
        return result;
    }

    // Parallel-filtering alternative (requires C++20, Threads::Threads, and
    // <algorithm>, <exception>, and <thread>). Replace the sequential scan below
    // when enabling this code; merge matches in chunk order and apply offset,
    // limit, and projection afterward. Unlike the current scan, this evaluates
    // every row even when a limit could stop the query early.
    /*
    if (rows.empty()) {
        return result;
    }

    const auto workerCount{
        std::min(rows.size(),
                 std::size_t{std::max(1u, std::thread::hardware_concurrency())})
    };

    std::vector<std::vector<std::size_t>> matches(workerCount);
    std::vector<std::exception_ptr> errors(workerCount);

    {
        std::vector<std::jthread> workers{};
        workers.reserve(workerCount);

        for (std::size_t worker{0}; worker < workerCount; ++worker) {
            const auto begin{worker * rows.size() / workerCount};
            const auto end{(worker + 1) * rows.size() / workerCount};

            workers.emplace_back([&, worker, begin, end] {
                try {
                    for (auto index{begin}; index < end; ++index) {
                        bool include{true};

                        for (const auto& resolved : predicates) {
                            if (!evaluatePredicate(
                                    *resolved.predicate,
                                    resolved.column,
                                    rows[index])) {
                                include = false;
                                break;
                            }
                        }

                        if (include) {
                            matches[worker].push_back(index);
                        }
                    }
                } catch (...) {
                    errors[worker] = std::current_exception();
                }
            });
        }
    } // jthread destructors join every worker

    for (const auto& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
    */

    std::size_t skipped{0};

    for (const auto& row : rows) {
        bool include{true};
        for (const auto& resolved : predicates) {
            if (!evaluatePredicate(*resolved.predicate, resolved.column, row)) {
                include = false;
                break;
            }
        }
        if (!include) {
            continue;
        }
        if (skipped < query.offset) {
            ++skipped;
            continue;
        }

        result.rows.push_back(projectRow(row, projection));
        if (query.limit && result.rows.size() >= *query.limit) {
            break;
        }
    }
    return result;
}

// Evaluate one typed predicate against a row cell.
bool QueryExecutor::evaluatePredicate(const Predicate& predicate, ColumnId column, const Row& row) const {
    return std::visit([&](const auto& typedPredicate) -> bool {
        using PredicateType = std::decay_t<decltype(typedPredicate)>;
        if constexpr (std::is_same_v<PredicateType, IntegerPredicate>) {
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
        } else {
            const auto* value{std::get_if<std::vector<double>>(&row.cell(column))};
            if (!value) {
                throw QueryError{"row cell is not a vector"};
            }
            const auto actualDistance{distance(*value, typedPredicate.referenceVector, typedPredicate.metric)};
            return evaluateFloatComparison(actualDistance, typedPredicate.op, typedPredicate.threshold);
        }
    }, predicate);
}

// Copy the selected cells, or the whole row for an empty projection.
Row QueryExecutor::projectRow(const Row& row, const std::vector<ColumnId>& projection) const {
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
Schema QueryExecutor::projectSchema(const Schema& schema, const std::vector<ColumnId>& projection) const {
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
