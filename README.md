# Vector-Relational Database

A persistent C++20 database for tables that combine relational values and vector
embeddings. It supports exact filtering, exact nearest-neighbor search, and
approximate nearest-neighbor search through HNSW indexes.

Use the C++ API for queries and index management, or the CLI for basic table and
row operations. Data is stored locally as CSV; no database server is required.

## Current capabilities

| Area | Supported today |
| --- | --- |
| Tables | Create, list, inspect, and drop tables; persist schemas across restarts |
| Data types | `INTEGER` (`int64_t`), `TEXT` (`std::string`), and fixed-dimension `VECTOR(n)` (`std::vector<float>`) |
| Rows | Insert, read, replace, and delete rows using persistent internal `RowId`s |
| NULL values | Nullable columns, explicit NOT NULL constraints, `isNull`, and `isNotNull` |
| Filtering | Integer comparisons, text equality/inequality, and vector-distance comparisons, combined with AND |
| Scan queries | Column projection, offset, and limit; bounded batches and optional parallel filtering that preserves table order |
| Vector search | Euclidean and cosine distance; exact top-k search and optional HNSW acceleration |
| Vector indexes | Create, list, and drop indexes; persisted definitions and automatic rebuilding after writes |
| Tools | CLI, C++ demo, synthetic database generator, query benchmarks, and CTest suite |

Tables can contain multiple vector columns with different dimensions. Rows are
validated against their schemas. A user-defined column named `id` is independent
of the internal `RowId` returned by inserts.

## Build and test

Requirements: CMake 3.16 or newer and a C++20 compiler with thread support.
Python 3 is optional for the generator and Python-based tests. hnswlib v0.8.0
headers are vendored in the repository; builds do not download dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Use Debug when running tests: the suite uses C++ assertions. With a single-config
generator, executables are under `build/`. With a multi-config generator such as
Visual Studio, use `build/Debug/` and the `.exe` suffix on Windows.

| Build target | Purpose |
| --- | --- |
| `vrdb` | Database library to link into C++ applications |
| `vrdb_cli` | Command-line table and row operations |
| `vrdb_demo` | Standalone C++ API example |
| `vrdb_query_benchmark` | Sequential versus parallel scan benchmark |
| `vrdb_hnsw_benchmark` | Exact versus HNSW top-k benchmark |

## CLI quick start

The examples below use the single-config executable path. Each command opens the
same database directory and persists its changes.

```sh
./build/vrdb_cli ./example_db init
./build/vrdb_cli ./example_db create documents title:TEXT 'embedding:VECTOR(3)'
./build/vrdb_cli ./example_db insert documents 'vector databases' '[0.1,0.2,0.3]'
./build/vrdb_cli ./example_db list
./build/vrdb_cli ./example_db describe documents
./build/vrdb_cli ./example_db select documents
./build/vrdb_cli ./example_db update documents 1 'updated title' '[0.2,0.1,0.3]'
./build/vrdb_cli ./example_db select documents --csv
./build/vrdb_cli ./example_db delete documents 1
```

`insert` prints the allocated `RowId`; use that value for `update` and `delete`
(the example assumes the first insert into a new table returns `1`). Updates
replace the entire row, with values supplied in schema order.

`select` displays rows in a bordered table. `select --csv` exports rows with their
IDs and NULL bitmap, without table decoration. Schema metadata is stored
separately and is not included in the export.

Columns allow NULL by default. Append `:NOT_NULL` to disallow it, for example
`title:TEXT:NOT_NULL`. Supply `'\N'` as a value for NULL; `'\\N'` in a TEXT
column stores the literal text `\N`. Quote these arguments in PowerShell or a
POSIX shell. Empty text and the word `NULL` are ordinary text values.

The CLI currently exposes `init`, `list`, `describe`, `create`, `insert`, `update`,
`delete`, and full-table `select`. Rich filtering, nearest-neighbor search,
table dropping, and index management are available through the C++ API.

## C++ queries and vector search

Link your application to the `vrdb` CMake target and include
`db/database_manager.hpp`. This example creates a table, filters rows, and
searches an embedding column:

```cpp
#include "db/database_manager.hpp"

#include <cstdint>
#include <string>
#include <vector>

int main() {
    using namespace vrdb;

    DatabaseManager db{"./cpp_example_db"};
    if (!db.hasTable("documents")) {
        db.createTable("documents", Schema{{
            Column{"title", DataType::TEXT, 0, false},
            Column{"year", DataType::INTEGER},
            Column{"embedding", DataType::VECTOR, 3}
        }});
        db.insert("documents", Row{{
            std::string{"vector databases"}, std::int64_t{2026},
            std::vector<float>{0.1F, 0.2F, 0.3F}
        }});
        db.createVectorIndex({"documents_embedding", "documents", "embedding",
                              DistanceMetric::COSINE});
    }

    Query scan{};
    scan.table = "documents";
    scan.projection = {"title", "year"};
    scan.predicates = {
        integerComparison("year", ComparisonOperator::GREATER_THAN_OR_EQUAL, 2020),
        vectorDistance("embedding", DistanceMetric::COSINE,
                       {0.2F, 0.1F, 0.3F}, ComparisonOperator::LESS_THAN, 0.25)
    };
    scan.limit = 10;
    auto matches = db.select(scan);

    NearestNeighborQuery nearest{"documents", "embedding", {0.2F, 0.1F, 0.3F},
                                 DistanceMetric::COSINE, 10};
    nearest.projection = {"title"};
    auto neighbors = db.nearestNeighbors(nearest);
}
```

### Exact scans

`select` returns a `QueryResult` containing a schema, rows, and positionally
aligned `rowIds`. Empty projection selects all columns. Predicates use AND;
offset and limit apply to matching rows in table order. Use
`db.select(scan, QueryExecutionOptions{1})` for sequential execution, or the
default options for a hardware-based worker count.

Use `nullCell()` to construct NULL values. Typed comparisons against NULL,
including inequality and vector-distance comparisons, do not match. Explicit
`isNull("column")` and `isNotNull("column")` predicates work on every type.

### Nearest-neighbor search

`nearestNeighbors` returns up to `k` rows ordered by ascending distance, with
RowId used to break ties among retrieved candidates. Its result contains:

- `result`: the projected `QueryResult`, including RowIds.
- `distances`: positionally aligned `double` distances in the selected metric.
- `method`: `NearestNeighborMethod::EXACT` or `NearestNeighborMethod::HNSW`.

| Query configuration | Execution |
| --- | --- |
| `AUTO`, no predicates, matching index | Approximate HNSW search |
| `AUTO`, no matching index | Exact top-k scan |
| Any predicates | Exact top-k scan over matching rows |
| `NearestNeighborMode::EXACT` | Exact top-k scan |

Set `nearest.mode = NearestNeighborMode::EXACT` to force exact results. Add
existing predicates to `nearest.predicates` to search only matching rows. NULL
vectors are skipped. Search vectors require finite coordinates and matching
dimensions; cosine distance also requires nonzero vectors.

Index names are database-unique. Each `(table, column, metric)` can have one
index; use `listVectorIndexes()` and `dropVectorIndex(name)` to manage them.
Definitions persist across restarts. Graphs build on first use and rebuild on
the next eligible search after a write. HNSW does not accelerate `select`.

See the [HNSW guide](docs/hnsw.md) for tuning, validation, index lifecycle, and
recall tests.

## Storage and architecture

```text
my_database/
|-- catalogs/
|   `-- documents.csv       # Table schema
|-- tables/
|   |-- documents.csv       # Rows, internal IDs, and NULL bitmap
|   `-- documents.nextid    # Persistent RowId allocation counter
`-- indexes/               # Created when an index is defined
    `-- documents_embedding.csv
```

Catalog files determine which tables exist. Row-oriented CSV preserves text
containing commas, quotes, and newlines, and round-trips float vector values.
The ID counter prevents ID reuse after deletion. Only index definitions are
stored on disk; HNSW graphs and row caches are held in memory.

| Component | Responsibility |
| --- | --- |
| `DatabaseManager` | Public table, row, query, and index APIs |
| `Catalog` and logical types | Table metadata and schema/row validation |
| `FileStorageEngine` | Filesystem operations, row mutations, and table cursors |
| `CsvFormat` and `IndexFormat` | Stream encoding for data and metadata |
| `Page` and `QueryExecutor` | Bounded row batches, predicate evaluation, and projection |
| `IndexManager` and `VectorIndex` | Index lifecycle, shared row caches, and an interchangeable HNSW adapter |

API failures use `DatabaseError` and its `SchemaError`, `StorageError`, and
`QueryError` subclasses. Table, column, and index names are case-sensitive;
table and index names must be valid identifiers containing letters, digits, and
underscores, with no leading digit.

## Benchmarks and sample data

Use an optimized build for timing:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
./build-release/vrdb_query_benchmark 30000 256 3 8
./build-release/vrdb_hnsw_benchmark 10000 16 100 50
```

For multi-config generators, use `build-release/Release/`. The scan benchmark
compares sequential and parallel filtering. The HNSW benchmark reports cold
build time plus the first query, warm search latency, exact scan latency,
recall@10, and resident memory where supported. Timing results depend on the
machine and dataset.

Generate a separate synthetic database with Python:

```sh
python scripts/generate_database.py 0.01 --output ./data/sample
```

The size is in decimal GB and applies to the table CSV. The generator refuses
to overwrite an existing destination. Its repeated embeddings make it useful
for storage and scan workloads; use the HNSW benchmark for similarity testing.

## Current limitations

- No SQL parser, interactive REPL, joins, aggregation, schema alteration,
  relational indexes, or general-purpose sorting API.
- No transactions or write-ahead log. Updates and deletes rewrite the table
  CSV; inserts also read the existing table. CSV storage favors inspectability
  over write throughput and random access.
- HNSW search is approximate. Filtered nearest-neighbor queries currently use
  exact scans, and writes invalidate graphs rather than updating them incrementally.
- Indexed tables must fit in memory: indexes share a full-row cache per table,
  and each built graph also stores its vectors and links. There is no cache eviction.
- Database operations must be externally serialized. Internal parallel scan
  workers do not provide concurrency control for simultaneous API calls,
  database owners, or external file edits.

## Further documentation

- [Storage format](docs/storage-format.md): CSV layouts, NULL encoding, and legacy compatibility.
- [HNSW guide](docs/hnsw.md): vector-search behavior, tuning, persistence, and benchmarks.
- [High-level proposal](docs/high-level-proposal.md): original project goals and future direction.
- [Milestone 1 proposal](docs/milestone1-design-proposal.md): initial module and API design.
- [Roadmap](docs/roadmap.md): project checklist; some status entries predate current capabilities.
