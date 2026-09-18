#include "types/schema.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename Function>
void expectInvalidArgument(Function&& function, const std::string& expectedMessagePart) {
    bool threw = false;
    try {
        std::forward<Function>(function)();
    } catch (const std::invalid_argument& error) {
        threw = true;
        assert(std::string(error.what()).find(expectedMessagePart) != std::string::npos);
    }
    assert(threw);
}

template <typename Function>
void expectOutOfRange(Function&& function, const std::string& expectedMessagePart) {
    bool threw = false;
    try {
        std::forward<Function>(function)();
    } catch (const std::out_of_range& error) {
        threw = true;
        assert(std::string(error.what()).find(expectedMessagePart) != std::string::npos);
    }
    assert(threw);
}

} // namespace

int main() {
    using namespace vrdb;

    Schema schema({
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
    assert(schema.columnId("review") == ColumnId{1});
    assert(schema.columnId("embedding") == ColumnId{3});
    assert(!schema.columnId("missing"));

    assert(schema.column(ColumnId{0}).name == "year");
    assert(schema.column(ColumnId{1}).name == "review");
    assert(schema.column(ColumnId{2}).name == "small_embedding");

    expectInvalidArgument(
        [] { static_cast<void>(VectorType{0}); },
        "Vector dimension must be greater than zero");
    expectInvalidArgument(
        [] { static_cast<void>(Schema(std::vector<Column>{})); },
        "Schema must contain at least one column");
    expectInvalidArgument(
        [] {
            static_cast<void>(Schema({
                Column("embedding", VectorType{384}),
                Column("embedding", TextType{}),
            }));
        },
        "Duplicate column name: embedding");
    expectInvalidArgument(
        [] { static_cast<void>(Column("", Int64Type{})); },
        "Column name cannot be empty");
    expectInvalidArgument(
        [] {
            Column column("temporary", Int64Type{});
            column.name.clear();
            static_cast<void>(Schema({std::move(column)}));
        },
        "Column name cannot be empty");

    Schema caseSensitiveSchema({
        Column("Embedding", VectorType{1}),
        Column("embedding", VectorType{1}),
    });
    assert(caseSensitiveSchema.columnId("Embedding") == ColumnId{0});
    assert(caseSensitiveSchema.columnId("embedding") == ColumnId{1});
    assert(!caseSensitiveSchema.columnId("EMBEDDING"));

    expectOutOfRange(
        [&schema] { static_cast<void>(schema.column(ColumnId{5})); },
        "ColumnId 5 is out of range");

    return 0;
}
