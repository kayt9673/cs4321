# CSV Storage Format

This document describes the current development storage format. It prioritizes
correctness, restart recovery, and human inspection over performance.

## Directory Layout

```text
database_directory/
├── catalog.csv
└── tables/
    └── <table_name>.csv
```

Constructing `Database(path)` creates the directory, an empty `catalog.csv`, and
the `tables/` directory when they do not exist. Opening the same path later
reloads schemas from the catalog and rows from each table CSV.

Table names are case-sensitive identifiers. They contain only letters, digits,
and underscores, and the first character must be a letter or underscore. This
keeps table names safe to use as file names.

## Catalog

`catalog.csv` has these columns:

```text
format_version,table_name,column_index,column_name,data_type,vector_dimension
```

The current `format_version` is `1`. There is one record per logical column.
`column_index` preserves declaration order. `vector_dimension` is empty for
INTEGER and TEXT and is a positive
integer for VECTOR. The catalog is written to a temporary file and renamed into
place so a partially written catalog is not exposed on successful replacement.

## Table Files

Each table is stored as one row-oriented CSV file. The first record contains the
column names in schema order. Remaining records contain row values in the same
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
- `RowId` exists in the logical/physical boundary but is not allocated or
  persisted yet.
- CSV is suitable for this sequential-scan milestone, not for random access or
  high-throughput vector search.
- The earlier experimental `.vrdb` line format is not migrated automatically.
  Databases created with that format must be recreated or migrated explicitly.
