#!/usr/bin/env python3
"""Generate a synthetic table of the requested size in decimal GB.

Size includes the header and rows in tables/documents.csv, excluding catalogs/documents.csv
and filesystem overhead. One GB is 1,000,000,000 bytes.
Rows have unique integer IDs, text, and deterministic 384-dimensional embeddings.
Only 256 distinct embeddings are used; this is a size/scan fixture, not a realistic
similarity benchmark. Writes use bounded batches and never overwrite a database.
"""

import argparse
import csv
from decimal import Decimal, InvalidOperation
from pathlib import Path

DIMENSION = 384
BATCH_BYTES = 8 * 1024 * 1024
HEADER = b'"id","title","embedding"\n'


# Encode one synthetic CSV row, optionally padding its title to fill the target size.
def row_bytes(document_id, embedding, padding=0):
    title = f"document-{document_id:012d}".ljust(128, "x") + "x" * padding
    return f'"{document_id:012d}","{title}","{embedding}"\n'.encode("ascii")


# Write an exact-sized table and its catalog without overwriting existing data.
def generate(destination, target_bytes):
    destination = destination.resolve()
    partial = destination.with_name(destination.name + ".partial")
    if destination.exists() or partial.exists():
        raise ValueError(f"refusing to overwrite {destination} or {partial}")
    embeddings = [
        "[" + ",".join(f"{((seed * 7919 + index * 1543) % 1000000) / 1000000:.6f}"
                        for index in range(DIMENSION)) + "]"
        for seed in range(256)
    ]
    row_size = len(row_bytes(1, embeddings[0]))
    row_count, padding = divmod(target_bytes - len(HEADER), row_size)
    if row_count < 1 or row_count > 999999999999:
        raise ValueError(f"size must accommodate 1 to 999999999999 rows ({row_size} bytes each)")
    (partial / "tables").mkdir(parents=True)
    (partial / "catalogs").mkdir()
    with (partial / "catalogs" / "documents.csv").open("w", newline="") as stream:
        writer = csv.writer(stream, quoting=csv.QUOTE_ALL, lineterminator="\n")
        writer.writerows([
            ["table_name", "column_index", "column_name", "data_type", "vector_dimension"],
            ["documents", 0, "id", "INTEGER", ""],
            ["documents", 1, "title", "TEXT", ""],
            ["documents", 2, "embedding", "VECTOR", DIMENSION],
        ])

    rows_per_batch = max(1, BATCH_BYTES // row_size)
    table = partial / "tables" / "documents.csv"
    with table.open("wb") as stream:
        stream.write(HEADER)
        for begin in range(1, row_count + 1, rows_per_batch):
            end = min(begin + rows_per_batch, row_count + 1)
            batch = b"".join(row_bytes(i, embeddings[(i - 1) % len(embeddings)],
                                       padding if i == row_count else 0)
                             for i in range(begin, end))
            stream.write(batch)
    if table.stat().st_size != target_bytes:
        raise RuntimeError("generated table size does not match requested size")
    if destination.exists():
        raise ValueError(f"destination appeared during generation: {destination}")
    partial.rename(destination)
    print(f"Created {destination}: {row_count:,} rows, {target_bytes:,} table bytes")


# Convert a positive decimal GB argument into a whole number of bytes.
def parse_gb(value):
    try:
        size = Decimal(value)
    except InvalidOperation as error:
        raise argparse.ArgumentTypeError("size must be a positive number of GB") from error
    # The fixed-width IDs support fewer than four million GB of these rows.
    if not size.is_finite() or size <= 0 or size > 4_000_000:
        raise argparse.ArgumentTypeError("size must be positive and at most 4,000,000 GB")
    byte_count = size * 1_000_000_000
    if byte_count != byte_count.to_integral_value():
        raise argparse.ArgumentTypeError("size must represent a whole number of bytes")
    return int(byte_count)


# Parse generator arguments and report completion or errors.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gb", type=parse_gb,
                        help="table size in decimal GB, e.g. 1, 2, or 0.5")
    parser.add_argument("--output", type=Path, default=Path("data/benchmark"),
                        help="database directory (default: data/benchmark)")
    args = parser.parse_args()
    try:
        generate(args.output, args.gb)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Error: {error}\nPartial output, if present, was retained.\n")


if __name__ == "__main__":
    main()
