#include "db/errors.h"
#include "types/row.h"
#include "types/schema.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

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
        Column("embedding", VectorType{3}),
    });

    const Row validRow({
        std::int64_t{2024},
        std::string{"hello"},
        VectorValue{1.0f, 2.0f, 3.0f},
    });
    schema.validateRow(validRow);

    expectSchemaError(
        [&schema] { schema.validateRow(Row({})); },
        "row expects 3 values but received 0");
    expectSchemaError(
        [&schema] { schema.validateRow(Row({std::int64_t{2024}, std::string{"hello"}})); },
        "row expects 3 values but received 2");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::int64_t{2024},
                std::string{"hello"},
                VectorValue{1.0f, 2.0f, 3.0f},
                std::int64_t{4},
            }));
        },
        "row expects 3 values but received 4");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::string{"2024"},
                std::string{"hello"},
                VectorValue{1.0f, 2.0f, 3.0f},
            }));
        },
        "column 'year' expects INTEGER but received TEXT");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                VectorValue{2024.0f},
                std::string{"hello"},
                VectorValue{1.0f, 2.0f, 3.0f},
            }));
        },
        "column 'year' expects INTEGER but received VECTOR");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::int64_t{2024},
                std::int64_t{5},
                VectorValue{1.0f, 2.0f, 3.0f},
            }));
        },
        "column 'review' expects TEXT but received INTEGER");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::int64_t{2024},
                std::string{"hello"},
                std::int64_t{3},
            }));
        },
        "column 'embedding' expects VECTOR but received INTEGER");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::int64_t{2024},
                std::string{"hello"},
                VectorValue{1.0f, 2.0f},
            }));
        },
        "vector column 'embedding' expects dimension 3 but received 2");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row({
                std::int64_t{2024},
                std::string{"hello"},
                VectorValue{1.0f, 2.0f, 3.0f, 4.0f},
            }));
        },
        "vector column 'embedding' expects dimension 3 but received 4");

    schema.validateRow(Row({
        std::int64_t{-1},
        std::string{},
        VectorValue{-1.0f, -2.0f, -3.0f},
    }));
    schema.validateRow(Row({
        std::int64_t{0},
        std::string{"zero vector"},
        VectorValue{0.0f, 0.0f, 0.0f},
    }));

    const Schema integerBoundsSchema({
        Column("minimum", Int64Type{}),
        Column("maximum", Int64Type{}),
    });
    integerBoundsSchema.validateRow(Row({
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
    }));

    const Row identicalValues({std::int64_t{7}, std::string{"same"}, VectorValue{0.0f, 0.0f, 0.0f}});
    const StoredRow first{RowId{100}, identicalValues};
    const StoredRow second{RowId{101}, identicalValues};
    assert(first.id != second.id);
    assert(first.row.values() == second.row.values());

    return 0;
}
