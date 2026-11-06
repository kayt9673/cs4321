# CSV Storage Format

This document describes the current development storage format. It prioritizes
correctness, restart recovery, and human inspection over performance.

`CsvFormat` handles this encoding through streams, using the existing
serialization helpers. `FileStorageEngine` owns paths and file operations and
delegates encoding to that format object. `FileStorageEngine` exposes
rows and schemas; neither it nor `Catalog` exposes CSV fields or filenames.

## Directory Layout

```text
database_directory/
├── catalogs/
│   └── <table_name>.csv
└── tables/
    ├── <table_name>.csv
    └── <table_name>.nextid
```

Constructing `DatabaseManager{path}` creates the database directory and empty `catalogs/` and
`tables/` directories when they do not exist. Opening the same path later
reloads schemas from the catalog and rows from each table CSV.

Table names are case-sensitive identifiers. They contain only letters, digits,
and underscores, and the first character must be a letter or underscore. This
keeps table names safe to use as file names. Column names are nonempty and
contain only ASCII letters, digits, and underscores.

## Table Catalogs

Each table has its own `catalogs/<table_name>.csv` with these columns:

```text
table_name,column_index,column_name,data_type,vector_dimension,nullable
```

There is one record per logical column of that table. Every `table_name` field
must match the catalog filename. Startup discovers tables from these catalogs;
a data file by itself does not register a table.
`column_index` preserves declaration order. `vector_dimension` is empty for
INTEGER and TEXT and is a positive
integer for VECTOR. Only the affected table’s catalog is written or removed.
A write uses a temporary file and renames it into place. Catalog and data
updates are not transactional.

`nullable` must be exactly `true` or `false`. Readers also accept the old
five-field catalog, interpreting every column as NOT NULL. Newly constructed
columns default to nullable, but existing tables always use their catalog-owned
schema. Table upgrades never change catalog constraints. Catalog detection and
table detection are independent, so an old catalog can govern a bitmap table.

Databases using the previous shared root catalog must be migrated before opening:
split its records by table name into `catalogs/<table_name>.csv`, removing the
old `format_version` field, then archive the root file. Table data files can
remain in the old format until their first write.

## Table Files

Each table is stored as one row-oriented CSV file. The first record contains
`__vrdb_row_id,__vrdb_null_bitmap` followed by the column names in schema order.
Remaining records contain the RowId, bitmap, and row cells in the same order.
The `.nextid` file prevents
reuse of deleted IDs. Older table CSVs without RowIds remain readable and are
upgraded on their first write.

Tables with RowIds but without a bitmap also remain readable and upgrade on
their next successful mutation. Both old layouts contain only present values;
empty TEXT and text `NULL` are never inferred to be missing. New schemas cannot
use either physical metadata name, while old schemas containing those names
remain readable through full positional header validation. A header-only file
is a valid empty table. Missing files and missing/malformed headers are errors.

The bitmap has exactly one `0` or `1` per logical column: `1` means NULL and
requires an empty payload; `0` means present. Present payloads use the ordinary
type encoding. For a nullable TEXT column these are distinct:

```text
"__vrdb_row_id","__vrdb_null_bitmap","value"
"1","1",""
"2","0",""
"3","0","NULL"
"4","0","\N"
```

`serializeRowForCsv` serializes payloads only; pair it with
`serializeNullBitmap` whenever NULL must round-trip. `CsvFormat` handles this
automatically. `select --csv` uses the same encoding but names the physical ID
`row_id`. The table reader accepts that alias only in the bitmap layout, with
the corresponding schema supplied separately. Exports contain no catalog
metadata and there is no CLI import command.

Before inserting, reserve the next-ID counter through temporary-file replacement,
then publish the row or upgraded table. Failed operations can consume IDs; gaps
are expected. Updates, deletes, retries, and upgrades never lower the established
counter. Legacy no-ID tables use synthesized row identities and initialize a
counter to at least row count plus one, honoring any higher valid sidecar from
an interrupted upgrade. Historical deleted-ID reuse cannot be ruled out where
allocation history was never recorded. Once established, the high-water mark
governs all subsequent allocation. Physical-ID tables reject missing, malformed,
or stale counters rather than inferring allocation history from surviving rows.

Temporary files are not recovery authorities: incomplete or unpublished `.tmp`
files are ignored on read and replaced on retry. Publication uses POSIX rename
or Windows `ReplaceFileW` for existing files (`MoveFileExW` for initial creation),
without deleting the destination first. These paths cover process interruption
and checked I/O failures on the tested filesystems, not power-loss durability;
there is no fsync protocol or multi-file transaction.

- INTEGER is stored as a base-10 signed integer.
- TEXT accepts empty strings, spaces, punctuation, quotes, and newlines.
- VECTOR is stored as one field in bracket notation, for example
  `[0.1,-0.2,0.3]`.

The writer quotes every CSV field. Vector fields may contain commas, brackets,
and numeric punctuation. On read, the table header, row width, value types,
and vector dimensions are checked against the catalog schema.

## Deliberate Limitations

- Row updates and deletes rewrite the table CSV; there is no WAL or transaction protocol.
- CSV is suitable for this sequential-scan milestone, not for random access or
  high-throughput vector search.
- The earlier experimental `.vrdb` line format is not migrated automatically.
  Databases created with that format must be recreated or migrated explicitly.

## Vector precision

Vector coordinates use IEEE 754 binary32 (`float`) in memory. CSV writers use
`max_digits10` precision for float round trips. Distance calculations and
query thresholds use `double`.
