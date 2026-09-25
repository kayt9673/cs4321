"""Check generator output against the C++ storage reader."""

import csv
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CLI = sys.argv.pop(1)
GENERATOR = Path(__file__).resolve().parents[1] / "scripts" / "generate_database.py"


class GeneratorTest(unittest.TestCase):
    # Check independent catalogs, temporary-file handling, and malformed metadata.
    def test_table_catalogs(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "database"
            subprocess.run([CLI, str(destination), "create", "first", "id:INTEGER"],
                           check=True, capture_output=True)
            first = destination / "catalogs" / "first.csv"
            original = first.read_bytes()
            modified = first.stat().st_mtime_ns
            subprocess.run([CLI, str(destination), "create", "second", "text:TEXT"],
                           check=True, capture_output=True)
            self.assertEqual(first.read_bytes(), original)
            self.assertEqual(first.stat().st_mtime_ns, modified)
            self.assertFalse((destination / "catalog.csv").exists())
            (destination / "catalogs" / "unfinished.csv.tmp").write_text("incomplete")
            result = subprocess.run([CLI, str(destination), "list"],
                                    check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.splitlines(), ["first", "second"])
            first.write_bytes(original.replace(b'"first"', b'"second"'))
            result = subprocess.run([CLI, str(destination), "list"], capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    # Check exact size, CLI compatibility, and protection against overwriting data.
    def test_generated_database(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "database"
            command = [sys.executable, str(GENERATOR), "0.00001",
                       "--output", str(destination)]
            result = subprocess.run(command, check=True, capture_output=True, text=True)
            self.assertEqual(len(result.stdout.splitlines()), 1)
            table = destination / "tables" / "documents.csv"
            self.assertEqual(table.stat().st_size, 10_000)

            with (destination / "catalogs" / "documents.csv").open() as stream:
                catalog = list(csv.reader(stream))
            self.assertEqual(catalog[0], ["table_name", "column_index", "column_name",
                                          "data_type", "vector_dimension"])
            self.assertEqual(len(catalog), 4)

            selected = subprocess.run([CLI, str(destination), "select", "documents"],
                                      check=True, capture_output=True, text=True)
            rows = list(csv.reader(io.StringIO(selected.stdout)))
            self.assertEqual(rows[0], ["id", "title", "embedding"])
            self.assertEqual(len(rows), 3)
            for row_id, row in enumerate(rows[1:], 1):
                self.assertEqual(int(row[0]), row_id)
                self.assertEqual(len(row[2][1:-1].split(",")), 384)

            original = table.read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(table.read_bytes(), original)

    # Check that invalid sizes fail without creating a database.
    def test_invalid_sizes(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "database"
            for size in ["0", "-1", "nan", "inf", "abc", "0.0000000001", "0.000000001"]:
                with self.subTest(size=size):
                    result = subprocess.run(
                        [sys.executable, str(GENERATOR), size, "--output", str(destination)],
                        capture_output=True,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
