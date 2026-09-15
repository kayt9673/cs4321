# Vector-Relational Database

This project is the starting point for a small persistent database implemented
from scratch in C++. The long-term goal is to support relational columns and
vector columns in the same table, enabling queries that combine ordinary integer
predicates with vector-distance predicates.

The project intentionally does not include a SQL parser, indexing algorithms,
transactions, joins, or an external database dependency yet.

## Current Architecture

- `Database`: top-level API for creating tables, inserting rows, and reading
  persisted rows.
- `Table`: owns a table name, schema, and in-memory rows used by the current
  query executor.
- `StorageEngine`: persistence abstraction. `FileStorageEngine` currently uses a
  simple line-oriented file format under the configured data directory.
- `Schema`, `Column`, `Row`, and `Value`: core type-safe relational data model.
- `Predicate`, `Query`, and `QueryExecutor`: programmatic query representation
  and a minimal sequential-scan executor.
- `vector/distance`: Euclidean and cosine distance utilities with dimension
  validation.

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

## Roadmap

- Phase 1: Core types and project structure
- Phase 2: Persistent row storage
- Phase 3: Integer predicates and sequential scans
- Phase 4: Vector-distance predicates
- Phase 5: Combined relational + vector queries
- Phase 6: Dataset/embedding ingestion
- Phase 7: Indexing and query optimization

## Next Tasks
1. Add catalog/schema persistence so tables can be reopened after restart.
2. Expand sequential scans to read from storage instead of only in-memory table rows.
3. Add richer predicate tests, including invalid column/type cases.
4. Define a stable on-disk row format version and migration boundary.
5. Add basic dataset ingestion for rows with embedding vectors.

push test
