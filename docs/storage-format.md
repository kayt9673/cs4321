# CSV Storage Format

This document describes the current development storage format. It prioritizes
correctness, restart recovery, and human inspection over performance.

`CsvFormat` handles this encoding through streams, using the existing
serialization helpers. `FileStorageEngine` owns paths and file operations and
delegates encoding to that format object. The `StorageEngine` interface exposes
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
reloads schemas from the catalogs; queries read rows from the table CSVs.

Table names are case-sensitive identifiers. They contain only letters, digits,
and underscores, and the first character must be a letter or underscore. This
keeps table names safe to use as file names. Column names are nonempty and
contain only ASCII letters, digits, and underscores.

## Table Catalogs

Each table has its own `catalogs/<table_name>.csv` with these columns:

```text
table_name,column_index,column_name,data_type,vector_dimension
```

There is one record per logical column of that table. Every `table_name` field
must match the catalog filename. Startup discovers tables from these catalogs;
a data file by itself does not register a table.
`column_index` preserves declaration order. `vector_dimension` is empty for
INTEGER and TEXT and is a positive
integer for VECTOR. Only the affected table’s catalog is written or removed.
A write uses a temporary file and renames it into place. Catalog and data
updates are not transactional.

Databases using the previous shared root catalog must be migrated before opening:
split its records by table name into `catalogs/<table_name>.csv`, removing the
old `format_version` field, then archive the root file. Table data files can
remain in the old format until their first write.

## Table Files

Each table is stored as one row-oriented CSV file. The first record contains
`__vrdb_row_id` followed by the column names in schema order. Remaining records
contain the RowId and row cells in the same order. The `.nextid` file prevents
reuse of deleted IDs, which cannot always be inferred from the remaining rows.
The counter is advanced before an append, so a failed append may leave an ID gap.
Older table CSVs without RowIds remain readable and are upgraded on their first write.

The synthetic database generator writes this older table format and does not
create a `.nextid` file. Its first row mutation upgrades the table.

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
