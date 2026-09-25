# CSV Storage Format

This document describes the current development storage format. It prioritizes
correctness, restart recovery, and human inspection over performance.

## Directory Layout

```text
database_directory/
├── catalogs/
│   └── <table_name>.csv
└── tables/
    └── <table_name>.csv
```

Constructing `DatabaseManager{path}` creates the database directory and empty `catalogs/` and
`tables/` directories when they do not exist. Opening the same path later
reloads schemas from the catalog and rows from each table CSV.

Table names are case-sensitive identifiers. They contain only letters, digits,
and underscores, and the first character must be a letter or underscore. This
keeps table names safe to use as file names.

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
A write opens the table’s catalog directly with truncation and replaces its
contents. No temporary catalog is created. An interrupted or failed write can
leave an incomplete catalog. Catalog and data updates are not transactional.

Databases using the previous shared root catalog must be migrated before opening:
split its records by table name into `catalogs/<table_name>.csv`, keeping the same
header, then archive the original root file outside the active catalog path.
Table data files do not need to change.

## Table Files

Each table is stored as one row-oriented CSV file. The first record contains the
column names in schema order. Remaining records contain row cells in the same
order.

- INTEGER is stored as a base-10 signed integer.
- TEXT is stored as the logical text value using CSV quote escaping.
- VECTOR is stored as one field in bracket notation, for example
  `[0.1,-0.2,0.3]`.

The writer quotes every CSV field and doubles embedded quote characters. The
reader accepts quoted or unquoted fields and supports embedded commas, quotes,
CR/LF characters, and empty strings. On read, the table header, row width,
value types, and vector dimensions are checked against the catalog schema.

## Deliberate Limitations

- Rows are appended directly; there is no WAL or transaction protocol.
- Row updates and deletes are not currently supported.
- CSV is suitable for this sequential-scan milestone, not for random access or
  high-throughput vector search.
- The earlier experimental `.vrdb` line format is not migrated automatically.
  Databases created with that format must be recreated or migrated explicitly.

## Vector precision

Vector coordinates use IEEE 754 binary64 (`double`) in memory. CSV writers use
`max_digits10` precision (17 significant digits) so finite coordinates can round
trip without losing precision. Distance calculations and query thresholds also
use `double`. Existing vector CSV fields remain readable; cells previously
rounded to float32 do not regain their original precision.
