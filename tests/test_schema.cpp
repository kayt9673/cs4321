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
        Column{"year", DataType::INTEGER},
        Column{"review", DataType::TEXT},
        Column{"small_embedding", DataType::VECTOR, 1},
        Column{"embedding", DataType::VECTOR, 384},
        Column{"large_embedding", DataType::VECTOR, 768},
    }};

    assert(schema.size() == 5);
    assert(!schema.empty());
    assert(isInteger(schema.column(std::size_t{0}).type));
    assert(isText(schema.column(std::size_t{1}).type));
    assert(isVector(schema.column(std::size_t{2}).type));
    assert(schema.column(std::size_t{2}).vectorDimension == 1);
    assert(schema.column(std::size_t{3}).vectorDimension == 384);
    assert(schema.column(std::size_t{4}).vectorDimension == 768);

    assert(schema.columnId("year") == std::size_t{0});
    assert(schema.columnId("embedding") == std::size_t{3});
    assert(!schema.columnId("missing"));
    assert(schema.column(std::size_t{0}).name == "year");
    assert(schema.column(std::size_t{2}).name == "small_embedding");

    expectSchemaError(
        [] { static_cast<void>(Column{"embedding", DataType::VECTOR, 0}); },
        "vector dimension must be greater than zero");
    expectSchemaError(
        [] { static_cast<void>(Schema{std::vector<Column>{}}); },
        "schema must contain at least one column");
    expectSchemaError(
        [] {
            static_cast<void>(Schema{{
                Column{"embedding", DataType::VECTOR, 384},
                Column{"embedding", DataType::TEXT},
            }});
        },
        "duplicate column name: embedding");
    expectSchemaError(
        [] { static_cast<void>(Column{"", DataType::INTEGER}); },
        "column name cannot be empty");

    const std::string allowed{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"};
    assert((Column{allowed, DataType::TEXT}.name == allowed));
    for (const auto invalid : {"has space", "has,comma", "has\"quote", "has\nnewline", "has-hyphen", "é"}) {
        expectSchemaError(
            [invalid] { static_cast<void>(Column{invalid, DataType::TEXT}); },
            "allows only ASCII letters, digits, and underscores");
    }

    const Schema caseSensitiveSchema{{
        Column{"Embedding", DataType::VECTOR, 1},
        Column{"embedding", DataType::VECTOR, 1},
    }};
    assert(caseSensitiveSchema.columnId("Embedding") == std::size_t{0});
    assert(caseSensitiveSchema.columnId("embedding") == std::size_t{1});
    assert(!caseSensitiveSchema.columnId("EMBEDDING"));

    expectSchemaError(
        [&schema] { static_cast<void>(schema.column(std::size_t{5})); },
        "column index 5 is out of range");

    return 0;
}
