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
- `StorageEngine`: persistence abstraction. `FileStorageEngine` currently uses a
  simple line-oriented file format under the configured database directory's
  `tables/` subdirectory.
- `storage/serialization`: row, value, and column encoding helpers shared by the
  catalog and file storage layers.
- `Schema`, `Column`, `Row`, and `Value`: core type-safe relational data model.
- `Predicate`, `Query`, `QueryResult`, and `QueryExecutor`: programmatic query
  representation and a sequential-scan executor with projection, offset, limit,
  integer predicates, text equality predicates, and vector-distance predicates.
- `vector/distance`: Euclidean and cosine distance utilities with dimension
  validation.

## Milestone 1 Functionality

- Database startup creates the database directory, initializes file storage
  under `tables/`, initializes `Catalog`, and loads existing table metadata.
- Table metadata is persisted in `catalog.vrdb`; table row files alone do not
  define recognized tables.
- Supported column/value types are `INTEGER` (`int64_t`), `TEXT`
  (`std::string`), and `VECTOR(n)` (`std::vector<float>`).
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

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
./build/vrdb_demo
```

On Windows with the default Visual Studio generator, the executable may be under
`build/Debug/vrdb_demo.exe`.

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Design Docs

- `docs/high-level-proposal.md`: project scope and intended end-to-end behavior.
- `docs/milestone1-design-proposal.md`: Milestone 1 module/API design.
