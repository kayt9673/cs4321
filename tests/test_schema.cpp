#include "db/errors.hpp"
#include "types/schema.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

// Check that an operation throws SchemaError containing the expected text.
template <typename Function>
void expectSchemaError(Function&& function, const std::string& expectedMessagePart) {
    bool threw{false};
    try {
        std::forward<Function>(function)();
    } catch (const vrdb::SchemaError& error) {
        threw = true;
        assert(std::string{error.what()}.find(expectedMessagePart) != std::string::npos);
    }
    assert(threw);
}

} // namespace

// Test schema lookup and rejection of invalid column definitions.
int main() {
    using namespace vrdb;

    const Schema schema{{
        Column{"year", ColumnType::INTEGER},
        Column{"review", ColumnType::TEXT},
        Column{"small_embedding", ColumnType::VECTOR, 1},
        Column{"embedding", ColumnType::VECTOR, 384},
        Column{"large_embedding", ColumnType::VECTOR, 768},
    }};

    assert(schema.size() == 5);
    assert(!schema.empty());
    assert(isInteger(schema.column(ColumnId{0}).type));
    assert(isText(schema.column(ColumnId{1}).type));
    assert(isVector(schema.column(ColumnId{2}).type));
    assert(schema.column(ColumnId{2}).vectorDimension == 1);
    assert(schema.column(ColumnId{3}).vectorDimension == 384);
    assert(schema.column(ColumnId{4}).vectorDimension == 768);

    assert(schema.columnId("year") == ColumnId{0});
    assert(schema.columnId("embedding") == ColumnId{3});
    assert(!schema.columnId("missing"));
    assert(schema.column(ColumnId{0}).name == "year");
    assert(schema.column(ColumnId{2}).name == "small_embedding");

    expectSchemaError(
        [] { static_cast<void>(Column{"embedding", ColumnType::VECTOR, 0}); },
        "vector dimension must be greater than zero");
    expectSchemaError(
        [] { static_cast<void>(Schema{std::vector<Column>{}}); },
        "schema must contain at least one column");
    expectSchemaError(
        [] {
            static_cast<void>(Schema{{
                Column{"embedding", ColumnType::VECTOR, 384},
                Column{"embedding", ColumnType::TEXT},
            }});
        },
        "duplicate column name: embedding");
    expectSchemaError(
        [] { static_cast<void>(Column{"", ColumnType::INTEGER}); },
        "column name cannot be empty");

    const Schema caseSensitiveSchema{{
        Column{"Embedding", ColumnType::VECTOR, 1},
        Column{"embedding", ColumnType::VECTOR, 1},
    }};
    assert(caseSensitiveSchema.columnId("Embedding") == ColumnId{0});
    assert(caseSensitiveSchema.columnId("embedding") == ColumnId{1});
    assert(!caseSensitiveSchema.columnId("EMBEDDING"));

    expectSchemaError(
        [&schema] { static_cast<void>(schema.column(ColumnId{5})); },
        "ColumnId 5 is out of range");

    return 0;
}
