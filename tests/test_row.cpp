#include "db/errors.hpp"
#include "types/row.hpp"
#include "types/schema.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

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

// Test row validation, integer bounds, and stored-row identities.
int main() {
    using namespace vrdb;

    const Schema schema{{
        Column{"year", ColumnType::INTEGER},
        Column{"review", ColumnType::TEXT},
        Column{"embedding", ColumnType::VECTOR, 3},
    }};

    const Row validRow{{
        std::int64_t{2024},
        std::string{"hello"},
        std::vector<double>{1.0, 2.0, 3.0},
    }};
    schema.validateRow(validRow);

    expectSchemaError(
        [&schema] { schema.validateRow(Row{{}}); },
        "row expects 3 cells but received 0");
    expectSchemaError(
        [&schema] { schema.validateRow(Row{{std::int64_t{2024}, std::string{"hello"}}}); },
        "row expects 3 cells but received 2");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::int64_t{2024},
                std::string{"hello"},
                std::vector<double>{1.0, 2.0, 3.0},
                std::int64_t{4},
            }});
        },
        "row expects 3 cells but received 4");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::string{"2024"},
                std::string{"hello"},
                std::vector<double>{1.0, 2.0, 3.0},
            }});
        },
        "column 'year' expects INTEGER but received TEXT");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::vector<double>{2024.0},
                std::string{"hello"},
                std::vector<double>{1.0, 2.0, 3.0},
            }});
        },
        "column 'year' expects INTEGER but received VECTOR");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::int64_t{2024},
                std::int64_t{5},
                std::vector<double>{1.0, 2.0, 3.0},
            }});
        },
        "column 'review' expects TEXT but received INTEGER");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::int64_t{2024},
                std::string{"hello"},
                std::int64_t{3},
            }});
        },
        "column 'embedding' expects VECTOR but received INTEGER");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::int64_t{2024},
                std::string{"hello"},
                std::vector<double>{1.0, 2.0},
            }});
        },
        "vector column 'embedding' expects dimension 3 but received 2");
    expectSchemaError(
        [&schema] {
            schema.validateRow(Row{{
                std::int64_t{2024},
                std::string{"hello"},
                std::vector<double>{1.0, 2.0, 3.0, 4.0},
            }});
        },
        "vector column 'embedding' expects dimension 3 but received 4");

    schema.validateRow(Row{{
        std::int64_t{-1},
        std::string{},
        std::vector<double>{-1.0, -2.0, -3.0},
    }});
    schema.validateRow(Row{{
        std::int64_t{0},
        std::string{"zero vector"},
        std::vector<double>{0.0, 0.0, 0.0},
    }});

    const Schema integerBoundsSchema{{
        Column{"minimum", ColumnType::INTEGER},
        Column{"maximum", ColumnType::INTEGER},
    }};
    integerBoundsSchema.validateRow(Row{{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
    }});

    const Row identicalCells{{std::int64_t{7}, std::string{"same"}, std::vector<double>{0.0, 0.0, 0.0}}};
    const StoredRow first{RowId{100}, identicalCells};
    const StoredRow second{RowId{101}, identicalCells};
    assert(first.id != second.id);
    assert(first.row.cells() == second.row.cells());

    return 0;
}
