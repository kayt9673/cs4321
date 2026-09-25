# vrdb Build Roadmap

A working checklist for the vector-relational database. 155 items across 17 phases.

**How to use this file**

- Tick items by changing `- [ ]` to `- [x]`. GitHub renders these as checkboxes; ticking one is a normal commit and shows up in a diff.
- Item numbers are **stable and global (1–155)**. Refer to work by number — "I'm on 24", "87 is blocked on 26". Never renumber. If you need to add work, append a decimal (`24.1`) rather than shifting everything below it.
- Sub-bullets under an item are individually tickable. An item is done when all of its sub-bullets are.
- Fill in the **Owner** line on a phase when someone picks it up.
- Phases are ordered by dependency, but several can overlap — see [Parallelism](#parallelism) at the bottom.

**Progress**

| Phase | Items | Done | Owner |
|---|---:|---:|---|
| [1. Lock Down Core Data Model](#phase-1-lock-down-core-data-model) | 1–7 | 7 / 7 | _team_ |
| [2. Catalog And Table Metadata](#phase-2-catalog-and-table-metadata) | 8–18 | 11 / 11 | _team_ |
| [3. Storage Format Correctness](#phase-3-storage-format-correctness) | 19–30 | 0 / 12 | _unassigned_ |
| [4. Database API](#phase-4-database-api) | 31–40 | 0 / 10 | _unassigned_ |
| [5. Query Representation](#phase-5-query-representation) | 41–52 | 0 / 12 | _unassigned_ |
| [6. Integer Predicates](#phase-6-integer-predicates) | 53–60 | 0 / 8 | _unassigned_ |
| [7. Text Predicates](#phase-7-text-predicates) | 61–67 | 0 / 7 | _unassigned_ |
| [8. Vector Distance Functions](#phase-8-vector-distance-functions) | 68–76 | 0 / 9 | _unassigned_ |
| [9. Vector Predicates](#phase-9-vector-predicates) | 77–86 | 0 / 10 | _unassigned_ |
| [10. Sequential Scan Executor](#phase-10-sequential-scan-executor) | 87–99 | 0 / 13 | _unassigned_ |
| [11. Programmatic Query API](#phase-11-programmatic-query-api) | 100–105 | 0 / 6 | _unassigned_ |
| [12. Optional Simple SQL Subset](#phase-12-optional-simple-sql-subset) | 106–115 | 0 / 10 | _unassigned_ |
| [13. CLI / Demo Tool](#phase-13-cli--demo-tool) | 116–122 | 6 / 7 | _team_ |
| [14. Import / Ingestion](#phase-14-import--ingestion) | 123–131 | 0 / 9 | _unassigned_ |
| [15. Error Handling And Result Types](#phase-15-error-handling-and-result-types) | 132–139 | 0 / 8 | _unassigned_ |
| [16. Durability Basics](#phase-16-durability-basics) | 140–147 | 0 / 8 | _unassigned_ |
| [17. Testing Infrastructure](#phase-17-testing-infrastructure) | 148–155 | 0 / 8 | _unassigned_ |
| **Total** | **1–155** | **24 / 155** | |

---

## Phase 1: Lock Down Core Data Model

**Owner:** _unassigned_

- [x] **1.** Define exact ownership and invariants for `Schema`, `Column`, `Row`, and `Cell`.
- [x] **2.** Add tests for valid/invalid schemas:
  - [x] empty schema
  - [x] duplicate column names
  - [x] vector column with dimension 0
  - [x] reject non-vector dimensions through column and schema validation
- [x] **3.** Add tests for valid/invalid rows:
  - [x] wrong number of cells
  - [x] wrong type in a column
  - [x] vector dimension mismatch
- [x] **4.** Add helper functions:
  - [x] `columnId(name)`
  - [x] `column(name).type`
  - [x] `validateRow(row)`
- [x] **5.** Decide whether column names are case-sensitive. Document it.
- [x] **6.** Add clear error messages for all validation failures.
- [x] **7.** Commit to the documented exception hierarchy.

---

## Phase 2: Catalog And Table Metadata

**Owner:** _unassigned_

- [x] **8.** Implement a persistent database catalog file.
- [x] **9.** Store table names and schemas in the catalog.
- [x] **10.** On database startup, load existing table metadata.
- [x] **11.** Prevent duplicate table creation across restarts.
- [x] **12.** Add `Database::listTables()`.
- [x] **13.** Add `Database::hasTable(name)`.
- [x] **14.** Add `Database::dropTable(name)` if your project scope allows it.
- [x] **15.** Add tests proving tables survive process restart.
- [x] **16.** Add tests for invalid table names.
- [x] **17.** Decide allowed table/column name characters.
- [x] **18.** Document catalog file format.

---

## Phase 3: Storage Format Correctness

**Owner:** _unassigned_

- [ ] **19.** Define a simple row file format clearly.
- [ ] **20.** Add file format versioning.
- [ ] **21.** Add escaping/encoding tests for text cells:
  - [ ] pipes
  - [ ] commas
  - [ ] percent signs
  - [ ] newlines
  - [ ] empty strings
- [ ] **22.** Add tests for integer edge cases:
  - [ ] `0`
  - [ ] negative numbers
  - [ ] large `int64_t`
- [ ] **23.** Add tests for vector edge cases:
  - [ ] negative floats
  - [ ] decimals
  - [ ] zero vectors
  - [ ] large dimensions
- [ ] **24.** Detect corrupted row files cleanly.
- [ ] **25.** Detect schema/row mismatch on read.
- [ ] **26.** Add append-only row persistence.
- [ ] **27.** Add a basic `flush()` if needed.
- [ ] **28.** Add a storage-level test that writes, destroys the DB object, recreates it, and reads rows back.
- [ ] **29.** Make sure storage creates directories if missing.
- [ ] **30.** Make sure storage fails cleanly if path is invalid.

---

## Phase 4: Database API

**Owner:** _unassigned_

- [ ] **31.** Finalize the public `Database` API.
- [ ] **32.** Implement:
  - [ ] `createTable`
  - [ ] `dropTable`
  - [ ] `insert`
  - [ ] `select`
  - [ ] `listTables`
  - [ ] `getSchema`
  - [ ] `rowCount`
- [ ] **33.** Make `insert` persist immediately.
- [ ] **34.** Make `select` read from persistent storage or a clearly synchronized table state.
- [ ] **35.** Add API tests for multiple tables.
- [ ] **36.** Add API tests for empty tables.
- [ ] **37.** Add API tests for unknown tables.
- [ ] **38.** Add API tests for schema mismatch on insert.
- [ ] **39.** Add API tests for restart behavior.
- [ ] **40.** Add examples in `main.cpp`.

---

## Phase 5: Query Representation

**Owner:** _unassigned_

- [ ] **41.** Expand `Query` into a real internal query plan object.
- [ ] **42.** Support selecting:
  - [ ] all columns
  - [ ] specific columns
- [ ] **43.** Add Projection support.
- [ ] **44.** Add Limit.
- [ ] **45.** Add optional Offset.
- [ ] **46.** Decide if predicates are always AND for now.
- [ ] **47.** Add AND predicate grouping explicitly.
- [ ] **48.** Leave OR for later unless easy.
- [ ] **49.** Add tests for empty predicate list.
- [ ] **50.** Add tests for multiple predicates.
- [ ] **51.** Add tests for unknown projected columns.
- [ ] **52.** Add tests for duplicate projected columns.

---

## Phase 6: Integer Predicates

**Owner:** _unassigned_

- [ ] **53.** Fully implement integer comparisons:
  - [ ] `=`
  - [ ] `!=`
  - [ ] `<`
  - [ ] `<=`
  - [ ] `>`
  - [ ] `>=`
- [ ] **54.** Add tests for every operator.
- [ ] **55.** Add tests for negative integers.
- [ ] **56.** Add tests for boundary cells.
- [ ] **57.** Add tests combining multiple integer predicates.
- [ ] **58.** Validate that integer predicates only apply to integer columns.
- [ ] **59.** Return useful errors for bad predicate types.
- [ ] **60.** Add documentation examples.

---

## Phase 7: Text Predicates

**Owner:** _unassigned_

- [ ] **61.** Decide what text predicates to support before indexing:
  - [ ] equality
  - [ ] inequality
  - [ ] maybe contains
- [ ] **62.** Implement text equality.
- [ ] **63.** Implement text inequality.
- [ ] **64.** Add tests for empty strings.
- [ ] **65.** Add tests for escaped/special characters.
- [ ] **66.** Validate text predicates only apply to text columns.
- [ ] **67.** Defer full-text search.

---

## Phase 8: Vector Distance Functions

**Owner:** _unassigned_

- [ ] **68.** Strengthen Euclidean distance tests.
- [ ] **69.** Strengthen cosine distance tests.
- [ ] **70.** Decide behavior for cosine distance on zero vectors.
- [ ] **71.** Add tests for dimension mismatch.
- [ ] **72.** Add tests for identical vectors.
- [ ] **73.** Add tests for opposite vectors.
- [ ] **74.** Add tests for orthogonal vectors.
- [ ] **75.** Add tests using realistic embedding dimensions, e.g. 384.
- [ ] **76.** Document distance semantics.

---

## Phase 9: Vector Predicates

**Owner:** _unassigned_

- [ ] **77.** Implement vector predicate representation cleanly:
  - [ ] column name
  - [ ] distance metric
  - [ ] reference vector
  - [ ] comparison operator
  - [ ] threshold
- [ ] **78.** Support at least:
  - [ ] `DISTANCE(...) < threshold`
  - [ ] `DISTANCE(...) <= threshold`
- [ ] **79.** Add enum for distance metric:
  - [ ] `EUCLIDEAN`
  - [ ] `COSINE`
- [ ] **80.** Validate vector predicates only apply to vector columns.
- [ ] **81.** Validate reference vector dimension matches column dimension.
- [ ] **82.** Add vector predicate executor tests.
- [ ] **83.** Add tests combining vector and integer predicates.
- [ ] **84.** Add tests where no rows match.
- [ ] **85.** Add tests where all rows match.
- [ ] **86.** Add tests where only one row matches.

---

## Phase 10: Sequential Scan Executor

**Owner:** _unassigned_

- [ ] **87.** Make executor scan rows from storage.
- [ ] **88.** Apply predicates row by row.
- [ ] **89.** Apply projection after filtering.
- [ ] **90.** Apply limit after filtering.
- [ ] **91.** Add result type, e.g. `QueryResult`.
- [ ] **92.** Include result schema in `QueryResult`.
- [ ] **93.** Include rows in `QueryResult`.
- [ ] **94.** Add tests for projection.
- [ ] **95.** Add tests for limit.
- [ ] **96.** Add tests for predicate order not changing results.
- [ ] **97.** Add tests for mixed column types.
- [ ] **98.** Add tests with hundreds/thousands of rows.
- [ ] **99.** Keep this intentionally unoptimized.

---

## Phase 11: Programmatic Query API

**Owner:** _unassigned_

- [ ] **100.** Make query construction ergonomic.
- [ ] **101.** Add helpers like:
  - [ ] `integerComparison("rating", ComparisonOperator::GREATER_THAN, 7)`
  - [ ] `vectorDistance("embedding", DistanceMetric::COSINE, ref, ComparisonOperator::LESS_THAN, 0.3f)`
- [ ] **102.** Add examples in README.
- [ ] **103.** Add example query in `main.cpp`.
- [ ] **104.** Add tests showing intended user-facing API.
- [ ] **105.** Keep SQL parsing out of this phase.

---

## Phase 12: Optional Simple SQL Subset

> Do this only after the programmatic API works.

**Owner:** _unassigned_

- [ ] **106.** Define a tiny SQL grammar.
- [ ] **107.** Support only:
  - [ ] `SELECT *`
  - [ ] `SELECT col1, col2`
  - [ ] `FROM table`
  - [ ] `WHERE` — integer predicates
  - [ ] `WHERE` — vector distance predicates
- [ ] **108.** Parse integer literals.
- [ ] **109.** Parse string literals.
- [ ] **110.** Parse vector literals.
- [ ] **111.** Parse `AND`.
- [ ] **112.** Convert SQL into `Query`.
- [ ] **113.** Add parser error messages.
- [ ] **114.** Add parser tests.
- [ ] **115.** Keep joins, aggregation, sorting, subqueries, and expressions out.

---

## Phase 13: CLI / Demo Tool

**Owner:** _unassigned_

- [x] **116.** Add a small command-line app.
- [x] **117.** Support creating a demo database.
- [x] **118.** Support inserting sample rows.
- [x] **119.** Support running a hardcoded query.
- [ ] **120.** Optionally support reading query JSON from a file.
- [x] **121.** Print query results as CSV.
- [x] **122.** Add README examples.

---

## Phase 14: Import / Ingestion

**Owner:** _unassigned_

- [ ] **123.** Add CSV ingestion for integer/text columns.
- [ ] **124.** Add vector parsing from CSV.
- [ ] **125.** Add JSONL ingestion if useful.
- [ ] **126.** Validate every row before insert.
- [ ] **127.** Report bad rows with line numbers.
- [ ] **128.** Support "fail fast" mode.
- [ ] **129.** Support "skip bad rows" mode.
- [ ] **130.** Add ingestion tests.
- [ ] **131.** Add small sample dataset in `examples/` or `data/sample/`.

---

## Phase 15: Error Handling And Result Types

**Owner:** _unassigned_

- [ ] **132.** Decide final error strategy.
- [ ] **133.** Make public API errors consistent.
- [ ] **134.** Avoid crashes from malformed user input.
- [ ] **135.** Add tests for bad database paths.
- [ ] **136.** Add tests for corrupted catalog files.
- [ ] **137.** Add tests for corrupted row files.
- [ ] **138.** Add tests for invalid queries.
- [ ] **139.** Improve exception messages or switch to `Result<T>`.

---

## Phase 16: Durability Basics

**Owner:** _unassigned_

- [ ] **140.** Ensure inserted rows are actually written to disk.
- [ ] **141.** Ensure files are closed/flushed correctly.
- [ ] **142.** Write rows atomically enough for this stage.
- [ ] **143.** Avoid partially written catalog updates where practical.
- [ ] **144.** Add a simple temporary-file-and-rename flow for catalog writes.
- [ ] **145.** Add restart tests after many inserts.
- [ ] **146.** Do not implement WAL yet.
- [ ] **147.** Document current durability limitations.

---

## Phase 17: Testing Infrastructure

**Owner:** _unassigned_

- [ ] **148.** Add CTest integration for all tests.
- [ ] **149.** Add one test executable per subsystem or a simple shared test runner.
- [ ] **150.** Add test utilities for temporary directories.
- [ ] **151.** Add test utilities for sample schemas.
- [ ] **152.** Add CI if you use GitHub.
- [ ] **153.** Add build instructions for Windows/macOS/Linux.
- [ ] **154.** Add sanitizer builds if available.
- [ ] **155.** Add a larger integration test:
  - [ ] create DB
  - [ ] create table
  - [ ] insert rows
  - [ ] restart
  - [ ] query
  - [ ] verify result

---

## Notes

These are observations from scoping the current tree against this list. They are not extra work items — they're context for whoever picks up the phase.

### Parallelism

The phases are dependency-ordered, but they are not all sequential. Three tracks can run alongside the main line from the start:

- **Phase 17** (test infrastructure) and **Phase 15** (error strategy) are needed *by* Phases 1–3, not after them. Item **150** in particular unblocks the restart tests in **15**, **28**, **39**, and **145**.
- **Phase 8** (distance functions) touches only `src/vector/distance.cpp` and depends on nothing else in the list. It can be done at any time by anyone.
- **Phases 6 and 7** (integer and text predicates) are independent of each other.

### Decisions to make early

Four items are decisions rather than code, and each one blocks work in a later phase. Settle them before the phase that depends on them starts:

| Decision | Blocks |
|---|---|
| **5.** Column name case sensitivity | 17, and every `columnIndex` call site |
| **7.** / **132.** Exceptions vs. `Result<T>` | The entire public API surface, Phases 4 and 15 |
| **17.** Allowed name characters | 16, and `FileStorageEngine::tablePath`, which turns a table name into a filesystem path |
| **46.** Whether predicates are always AND | 47, 48, and the shape of the executor in Phase 10 |

Item **7** and item **132** are the same decision made twice, eleven phases apart. Making it once, in Phase 1, is much cheaper than retrofitting it in Phase 15.

### Defects addressed from the original tree

- **Catalog-backed startup is now present.** `Database` loads table names and schemas from per-table `catalogs/<table>.csv` files, and row data lives in one CSV file per table under the database directory's `tables/` subdirectory.
- **A minimal CLI is now present.** `vrdb_cli` initializes or reopens a database, creates tables, inserts typed rows, lists and describes tables, and prints full-table scans as CSV. SQL parsing remains deferred.
- **Queries now read persisted rows through `Database::select()`.** The executor takes a schema and row set, returning `QueryResult` with projection support instead of reading a separate in-memory `Table`.

### Relation to the Phase 1 work split

The [Phase 1 CLD Deliverables](https://claude.ai/code/artifact/dff54209-195f-44a9-a6f4-b05890add817) document splits early work into file-disjoint streams for a team of 5–7. It was written against the README's older 7-phase roadmap, so its stream IDs map onto this list rather than matching it:

| Stream | Covers items |
|---|---|
| D0 — Interface freeze & file split | Prerequisite for all of Phase 1 |
| D1 — Error model | 6, 7, 59, 132, 133, 139 |
| D2 — Cell & type system | 1, 4 |
| D3 — Schema & column model | 1, 2, 4, 5, 9, 17 |
| D4 — Row & tuple semantics | 1, 3, 4 |
| D5 — Table & catalog | 8, 10, 11, 12, 13 |
| D6 — Row codec & format v1 | 19, 20, 21, 24, 25 |
| D7 — Build, CI & test harness | 148, 150, 152, 153, 154 |

This file is the source of truth for *what* gets built. That document is only about *how to divide the first week of it* across the team.
