# HNSW vector indexes

The HNSW integration adds explicit approximate nearest-neighbor search without
changing the logical row model, CSV table/catalog formats, or existing exact
`select` semantics. It uses the unmodified hnswlib v0.8.0 headers; provenance and
license are recorded in `third_party/hnswlib/UPSTREAM.md` and `LICENSE`. Builds
perform no dependency downloads.

## API and execution

Include `db/database_manager.hpp` for the complete public API:

```cpp
VectorIndexDefinition definition{"documents_embedding", "documents", "embedding",
                                  DistanceMetric::COSINE};
definition.options.M = 16;
definition.options.efConstruction = 200;
definition.options.efSearch = 50;
definition.options.seed = 100;
db.createVectorIndex(definition);
auto definitions = db.listVectorIndexes();

NearestNeighborQuery query{"documents", "embedding", referenceVector,
                            DistanceMetric::COSINE, 10};
query.projection = {"title"};
auto result = db.nearestNeighbors(query);

query.predicates = {textComparison("category", ComparisonOperator::EQUAL, "science")};
auto filtered = db.nearestNeighbors(query); // EXACT, including vector predicates
query.predicates.clear();
query.mode = NearestNeighborMode::EXACT;
auto baseline = db.nearestNeighbors(query);

db.dropVectorIndex("documents_embedding");
```

Index names are database-unique and use the same safe identifier rules as table
names. A table may have one index per `(column, metric)` pair; multiple vector
columns and different metrics are supported. Creation validates the definition
and infers its dimension from the schema, without reading table data. Supplying
a nonzero definition dimension requires it to match the schema. Listing returns
definitions sorted by name.

`NearestNeighborResult::result` is an ordinary owning `QueryResult`, and
`distances` corresponds positionally to its rows and RowIds. `method` identifies
the selected execution path. `AUTO` uses HNSW only when a matching index exists
and the query has no predicates. `EXACT`, filtered queries, and missing indexes
use bounded-batch scans with an O(k) top-k heap. Exact execution does not build
or populate an index cache.

Results sort by distance and then RowId. HNSW applies the RowId tie-break among
retrieved candidates; it does not guarantee the globally smallest IDs among tied
neighbors. Its result membership is approximate. Exact search guarantees the
nearest qualifying rows. Projection follows ranking. There is no separate limit
or offset. `k` greater than the population is clamped; `k = 0` validates the query
and returns a correctly projected empty result without building a graph. The
reported method for `k = 0` is the selected route. Empty and all-NULL populations
return no rows.

Whole-vector NULL values are skipped. Non-NULL search vectors require finite
coordinates and the schema's dimension. Cosine vectors must be nonzero. Invalid
indexed vectors fail graph construction with index/table/column/RowId context;
invalid vectors encountered in exact search after predicate filtering fail that
search. Invalid vectors are not silently removed. These additional checks apply
to the new search/index operations; existing storage and scan validation remain
unchanged.

## Modules and distance compatibility

- `VectorIndex` owns its indexed vectors and returns library-independent
  `VectorCandidate` values. `VectorEntry` views are only borrowed during build.
- `HnswVectorIndex` hides hnswlib in a PImpl and maps contiguous library labels
  to persistent 64-bit RowIds. Rebuilds insert in ascending RowId order with the
  configured seed. Reproducibility is within the same toolchain/library version;
  upstream random-engine behavior may differ across standard libraries.
- Its custom `SpaceInterface<double>` calls span-based overloads of the existing
  distance utilities. Existing vector overloads remain available. Euclidean
  results include the square root, and cosine uses the original formula with
  double accumulation. No normalized copies or squared-L2 units leak into results.
- `IndexManager` owns graphs and one full-row cache per indexed table. It has no
  references to `DatabaseManager`, so database moves transfer valid state. Graphs
  copy their vector data into hnswlib; columns do not duplicate the full row cache.
- `IndexFormat` encodes metadata streams; `FileStorageEngine` owns paths and file
  publication. Predicate validation, matching, and projection are shared with
  the existing executor.

Builds publish complete graphs only. A failed build never publishes a partial
graph; later searches retry. Runtime library errors become contextual
`QueryError`s. An index build/search error is reported to the caller rather than
silently switching to a different execution method.

## Persistence, invalidation, and memory

Each `indexes/<name>.csv` contains one escaped CSV record:

```text
vrdb_vector_index,1,name,table,column,metric,dimension,M,efConstruction,efSearch,seed
```

The record uses actual values, with metric `EUCLIDEAN` or `COSINE`. Definitions
are written to `.csv.tmp`, closed, and atomically published using the storage
engine's existing platform-specific replacement routine. Temporary files are
ignored during discovery. Missing `indexes/` means no indexes; malformed versions,
filename/name mismatches, duplicate targets, unknown tables/columns, and schema
dimension mismatches fail startup with a storage error. Old databases need no
migration. Startup loads definitions without constructing graphs or reading rows.

The first eligible query loads its table through existing cursors and builds
the requested graph. Other indexes on that table share the row cache. Before an
insert/update/delete attempts storage mutation, all that table's graphs and its
cache are discarded, even if the operation subsequently fails. The next eligible
query rebuilds. Dropping the last index releases the table cache. Table deletion
removes associated definitions before deleting table storage; if cleanup fails,
the table remains and some indexes may already have been removed.

The cache stores full rows, including TEXT and all vector columns. Each built
graph also stores a copy of its own vectors plus links, labels, and library
bookkeeping. Memory therefore grows with full table size and the number of built
indexes. There is no eviction or configured memory budget. Cold builds need
temporary entry arrays and additional construction memory. Results own their
projected data independently of caches and database lifetime.

Operations must be externally serialized, including reads, writes, index builds,
and database moves. hnswlib's `setEf` is mutable. Simultaneous database owners or
external edits to table files are not coordinated with live caches. The design
adds no WAL, transactions, or stronger cross-file crash guarantees than existing
storage.

## Tuning and verification

Defaults are `M=16`, `efConstruction=200`, `efSearch=50`, seed `100`.
Validation requires `2 <= M <= 100`, `efConstruction >= M`, and `efSearch > 0`.
Higher M increases connectivity and memory; higher construction breadth increases
build effort and often recall; higher search breadth increases query effort and
often recall. Search uses at least `max(efSearch, requested_count)`. Recreate the
definition to change persisted tuning values.

Run all regression and recall checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R hnsw_recall -V
```

The recall test uses 10,000 deterministic 16-dimensional vectors, 100 held-out
queries, and both metrics. It compares HNSW to exhaustive top-k sorting and checks
the production exact path against the same oracle. Acceptance is mean recall@10
at least 0.95 for each metric with default options. Other tests cover distance
compatibility, malformed input/metadata, NULLs, exact filtering, projection, ties,
large RowIds, restart, failed writes/builds, and ownership moves. Tests follow the
repository's assert-based convention; use a build without `NDEBUG`.

Use an optimized build for timing:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target vrdb_hnsw_benchmark
./build-release/vrdb_hnsw_benchmark 10000 16 100 50
```

Arguments are row count, dimension, query count, and efSearch. The benchmark uses
Euclidean top-k queries and reports cold build plus first-query time, warm HNSW
median/p95, exact CSV scan median, mean recall@10, and process resident memory
before/after the cold query on Linux and Windows. Resident-memory deltas include
allocator behavior and are not exact index-byte counts. Timings compare complete
database calls, including CSV parsing for exact scans. They have no pass/fail
threshold and should be interpreted separately from graph-only performance.

Future implementations can replace the `VectorIndex` adapter, add graph
snapshots behind index persistence, or add incremental maintenance behind the
index manager. CLI search commands, approximate filtering, graph snapshots,
incremental updates, and concurrent database operations are outside this version.
