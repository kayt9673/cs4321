# Vector-Relational Database

This project is the starting point for a persistent database implemented
from scratch in C++. The long-term goal is to support relational columns and
vector columns in the same table, enabling queries that combine ordinary integer
predicates with vector-distance predicates.

The project does not include a SQL parser, indexing algorithms,
transactions, joins, or an external database dependency yet.

## Current Architecture

- `Database`: top-level API for creating tables, inserting rows, and reading
  persisted rows through `select()` and `rowCount()`.
- `Catalog`: persistent table metadata store used to reload table names and
  schemas when the database opens.
- `StorageEngine`: persistence abstraction. `FileStorageEngine` stores one
  RFC 4180-style CSV file per table under the database directory's `tables/`
  subdirectory.
- `storage/serialization`: CSV record, row-cell, vector, and data-type encoding
  shared by the catalog and file storage layers.
- `Schema`, `Column`, `Row`, and `Cell`: strongly validated logical data model.
  Rows store `Cell` variants containing integers, strings, or 64-bit
  floating-point vectors.
  Columns declare a `ColumnType` and a vector dimension;
  validation rejects dimensions on non-vector columns.
- `ColumnId` and `RowId`: stable internal identifiers. Schemas resolve names to
  `ColumnId` through a map; `StoredRow` keeps physical identity separate from
  logical cells.
- `Predicate`, `Query`, `QueryResult`, and `QueryExecutor`: programmatic query
  representation and a sequential-scan executor with projection, offset, limit,
  integer predicates, text equality predicates, and vector-distance predicates.
- `vector/distance`: Euclidean and cosine distance utilities with dimension
  validation.

## Cell Cells

```cpp
Schema schema({
    Column("id", ColumnType::INTEGER),
    Column("title", ColumnType::TEXT),
    Column("embedding", ColumnType::VECTOR, 3),
});
Row row({
    std::int64_t{1},
    std::string{"example"},
    std::vector<double>{0.1, 0.2, 0.3},
});
schema.validateRow(row);
const auto& title = std::get<std::string>(row.cell(ColumnId{1}));
```

`Cell` is `std::variant<std::int64_t, std::string, std::vector<double>>`.
Copying a row copies its cells and payloads independently.
`std::get<T>()` accesses a payload and throws `std::bad_variant_access` for the
wrong type; `std::get_if<T>()` returns a pointer or null. Predicates also use a
variant, dispatched with `std::visit`.
Runtime column metadata remains necessary to load schemas from the CLI and disk.
The catalog and table CSV formats are unchanged by this C++ API change.

## Memory Ownership

`Database` directly contains its catalog and query executor, and exclusively
owns a heap-allocated storage engine through `std::unique_ptr`. Its members
release their owned memory automatically when the database is destroyed.
A database can be moved but cannot be copied. After moving a database, the
source may be destroyed or assigned a new database before reuse.

Row collections, row cells, schema columns, and vector coordinates already use
heap-backed `std::vector` storage, and catalog entries use `std::unordered_map`.
Local container handles and scalar temporaries can still live on the stack;
their size does not grow with the number of rows or vector dimensions. Query
results own their data and remain valid after the database is destroyed.
Queries currently materialize the entire table in memory before filtering.

## Milestone 1 Functionality

- Database startup creates the database directory, initializes file storage
  under `tables/`, initializes `Catalog`, and loads existing table metadata.
- Table metadata is persisted in `catalog.csv`; table row files alone do not
  define recognized tables.
- Supported column/cell types are `INTEGER` (`int64_t`), `TEXT`
  (`std::string`), and `VECTOR(n)` (`std::vector<double>`).
- Schemas preserve column order, require at least one column, require unique
  non-empty column names, and require vector dimensions only for vector columns.
- Rows are validated against schemas for width, type, and vector dimension.
- Inserts validate rows and write directly to persistent storage.
- Queries are programmatic `Query` objects with table name, projection,
  predicates, optional limit, and offset.
- Empty projection means all columns; non-empty projection returns a projected
  `QueryResult` schema and projected rows.
- Predicates use AND semantics and support integer comparison, text equality
  and inequality, and vector distance comparisons for Euclidean or cosine
  distance.
- Query execution reads persisted rows from storage, scans sequentially, applies
  predicates, offset, projection, and limit, then returns `QueryResult`.
- Errors use the following exception hierarchy: `DatabaseError`,
  `SchemaError`, `StorageError`, and `QueryError`.
- Column names are case-sensitive. Table names are case-sensitive identifiers
  containing letters, digits, and underscores, and must not begin with a digit.

## Database Directory Layout

Opening `Database("./my_database")` creates or reopens this layout:

```text
my_database/
├── catalog.csv
└── tables/
    ├── documents.csv
    └── reviews.csv
```

`catalog.csv` stores table names, column order, logical types, and vector
dimensions. Each table CSV has a header row followed by logical rows. TEXT
cells use CSV quote escaping, including embedded commas, quotes, and newlines.
A vector is stored in one CSV field such as `"[0.1,-0.2,0.3]"`.

The current storage is row-oriented because inserts and queries operate on
complete rows and execution uses sequential scans. A file per column would add
row-alignment and recovery complexity without helping the current workload.
The CSV format favors correctness and inspectability; it is not intended as the
final high-performance storage format.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Command-Line Interface

```sh
./build/vrdb_cli ./example_db init
./build/vrdb_cli ./example_db create documents \
  id:INTEGER title:TEXT 'embedding:VECTOR(3)'
./build/vrdb_cli ./example_db insert documents \
  1 'vector databases' '[0.1,-0.2,0.3]'
./build/vrdb_cli ./example_db list
./build/vrdb_cli ./example_db describe documents
./build/vrdb_cli ./example_db select documents
```

Every invocation reopens the database from disk, so the latter commands also
exercise catalog and row recovery. `select` currently performs an all-row query
and writes CSV to standard output. Rich predicates remain available through the
C++ `Query` API; the CLI intentionally does not include a SQL parser yet.

The separate `vrdb_demo` executable remains as a hard-coded API example.

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Design Docs

- `docs/high-level-proposal.md`: project scope and intended end-to-end behavior.
- `docs/milestone1-design-proposal.md`: Milestone 1 module/API design.
- `docs/storage-format.md`: current CSV catalog and table format.
