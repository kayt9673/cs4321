# CLI Demo Cheat Sheet

A ~15 minute live demo of the Milestone 1 CLI. Copy commands one at a time and
talk between them. Run everything from the repo root after building:

```sh
cmake -S . -B build && cmake --build build
```

Outputs below are shown without color; in a terminal, types and messages are
color-coded.

| # | Section | Time |
|---|---|---:|
| 1 | Setup | 2 min |
| 2 | Typed schemas | 2 min |
| 3 | Insert and query | 3 min |
| 4 | Persistence | 3 min |
| 5 | Validation | 3 min |
| 6 | Wrap-up | 2 min |

---

## 1. Setup (2 min)

```sh
rm -rf demo_db
./build/vrdb_cli demo_db init
./build/vrdb_cli demo_db create movies id:INTEGER title:TEXT 'embedding:VECTOR(3)'
```

**Say:** A table mixes ordinary relational columns with a vector column in the
same schema. The vector's dimension is part of its type.

---

## 2. Typed schemas (2 min)

```sh
./build/vrdb_cli demo_db list
./build/vrdb_cli demo_db describe movies
```

```text
id        INTEGER
title     TEXT
embedding VECTOR(3)
```

**Say:** Three supported types: `INTEGER`, `TEXT`, `VECTOR(n)`. Only vector
columns can carry a dimension — that's enforced by the type system, not a
runtime check.

---

## 3. Insert and query (3 min)

```sh
./build/vrdb_cli demo_db insert movies 1 'The Matrix' '[0.9,0.1,0.2]'
./build/vrdb_cli demo_db insert movies 2 'Toy Story' '[0.1,0.8,0.3]'
./build/vrdb_cli demo_db insert movies 3 'Inception, the movie' '[0.8,0.2,0.4]'
./build/vrdb_cli demo_db select movies --pretty
```

```text
┌────┬──────────────────────┬─────────────────┐
│ id │ title                │ embedding       │
├────┼──────────────────────┼─────────────────┤
│  1 │ The Matrix           │ [0.9, 0.1, 0.2] │
│  2 │ Toy Story            │ [0.1, 0.8, 0.3] │
│  3 │ Inception, the movie │ [0.8, 0.2, 0.4] │
└────┴──────────────────────┴─────────────────┘
3 rows
```

Then the machine-readable version:

```sh
./build/vrdb_cli demo_db select movies
```

**Say:** Every value is parsed against the column's type before it's stored.
`select` runs through the same query executor as the C++ API — a sequential
scan. Default output is CSV so it can be piped into other tools; `--pretty` is
for humans.

---

## 4. Persistence (3 min)

```sh
cat demo_db/catalog.csv
cat demo_db/tables/movies.csv
```

**Say:**
- Every command above was a **separate process**. Nothing lives in memory
  between commands — `select` found the rows because they were reloaded from
  disk through the catalog.
- `catalog.csv` holds table names and schemas; each table has its own CSV of
  rows. Note `"Inception, the movie"` is quoted, so commas in text are safe.
- CSV was chosen for correctness and inspectability, not speed.

---

## 5. Validation (3 min)

Each of these is rejected with a clear error:

```sh
./build/vrdb_cli demo_db insert movies 4 'Up' '[0.1,0.2]'
# Error: vector column 'embedding' expects dimension 3 but received 2

./build/vrdb_cli demo_db insert movies abc 'Up' '[0.1,0.2,0.3]'
# Error: invalid INTEGER value: abc

./build/vrdb_cli demo_db create movies id:INTEGER
# Error: table already exists: movies

./build/vrdb_cli demo_db create songs id:INTEGER id:TEXT
# Error: duplicate column name: id
```

**Say:** The CLI only parses text into typed values; schema, row, and catalog
rules are enforced in the core library (`Schema`, `Row`, `Catalog`), so every
entry point gets the same guarantees. Show that the bad
insert didn't land:

```sh
./build/vrdb_cli demo_db select movies --pretty
```

---

## 6. Wrap-up (2 min)

**Say:**
- **Done:** typed schemas, persistent catalog, CSV storage, sequential-scan
  queries, CLI.
- **Already in the C++ API but not the CLI:** integer and text predicates,
  vector-distance predicates (Euclidean and cosine), projection, limit, offset.
- **Next:** see `docs/roadmap.md` — storage hardening, exposing predicates, and
  possibly a small SQL subset.

---

## If asked

**"Why does the CSV show `0.899999976` instead of `0.9`?"**
Vectors are 32-bit floats. The file stores 9 significant digits so every value
round-trips exactly; `--pretty` rounds for display.

**"Can you filter?"**
Yes, through the C++ `Query` API. The CLI deliberately has no query language
yet.

**"What about text with newlines or long vectors?"**

```sh
./build/vrdb_cli demo_db create songs id:INTEGER lyrics:TEXT 'embedding:VECTOR(8)'
./build/vrdb_cli demo_db insert songs 1 $'Hello, "world"\nsecond line' '[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8]'
./build/vrdb_cli demo_db select songs --pretty
```

```text
│  1 │ Hello, "world"↵second line │ [0.1, 0.2, 0.3, 0.4, 0.5, …, 0.8] (8d) │
```

---

## Troubleshooting

- **Something went wrong mid-demo:** `rm -rf demo_db` and rerun section 1 and
  the inserts from section 3.
- **Colors not showing** (e.g. screen-sharing tool): `export CLICOLOR_FORCE=1`.
- **Colors unreadable on the projector:** `export NO_COLOR=1`.
- **Quote vectors and `VECTOR(n)` types**, and don't put spaces inside vectors —
  the shell would split them into separate arguments.
- Cleanup afterwards: `rm -rf demo_db`.
