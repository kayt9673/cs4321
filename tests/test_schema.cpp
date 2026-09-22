#include "db/errors.h"
#include "types/schema.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename Function>
void expectSchemaError(Function&& function, const std::string& expectedMessagePart) {
    bool threw = false;
    try {
        std::forward<Function>(function)();
    } catch (const vrdb::SchemaError& error) {
        threw = true;
        assert(std::string(error.what()).find(expectedMessagePart) != std::string::npos);
    }
    assert(threw);
}

} // namespace

int main() {
    using namespace vrdb;

    const Schema schema({
        Column("year", Int64Type{}),
        Column("review", TextType{}),
        Column("small_embedding", VectorType{1}),
        Column("embedding", VectorType{384}),
        Column("large_embedding", VectorType{768}),
    });

    assert(schema.size() == 5);
    assert(!schema.empty());
    assert(isInteger(schema.column(ColumnId{0}).type));
    assert(isText(schema.column(ColumnId{1}).type));
    assert(isVector(schema.column(ColumnId{2}).type));
    assert(std::get<VectorType>(schema.column(ColumnId{2}).type).dimension() == 1);
    assert(std::get<VectorType>(schema.column(ColumnId{3}).type).dimension() == 384);
    assert(std::get<VectorType>(schema.column(ColumnId{4}).type).dimension() == 768);

    assert(schema.columnId("year") == ColumnId{0});
    assert(schema.columnId("embedding") == ColumnId{3});
    assert(!schema.columnId("missing"));
    assert(schema.column(ColumnId{0}).name == "year");
    assert(schema.column(ColumnId{2}).name == "small_embedding");

    expectSchemaError(
        [] { static_cast<void>(VectorType{0}); },
        "vector dimension must be greater than zero");
    expectSchemaError(
        [] { static_cast<void>(Schema(std::vector<Column>{})); },
        "schema must contain at least one column");
    expectSchemaError(
        [] {
            static_cast<void>(Schema({
                Column("embedding", VectorType{384}),
                Column("embedding", TextType{}),
            }));
        },
        "duplicate column name: embedding");
    expectSchemaError(
        [] { static_cast<void>(Column("", Int64Type{})); },
        "column name cannot be empty");

    const Schema caseSensitiveSchema({
        Column("Embedding", VectorType{1}),
        Column("embedding", VectorType{1}),
    });
    assert(caseSensitiveSchema.columnId("Embedding") == ColumnId{0});
    assert(caseSensitiveSchema.columnId("embedding") == ColumnId{1});
    assert(!caseSensitiveSchema.columnId("EMBEDDING"));

    expectSchemaError(
        [&schema] { static_cast<void>(schema.column(ColumnId{5})); },
        "ColumnId 5 is out of range");

    return 0;
}
