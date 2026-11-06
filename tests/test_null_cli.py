"""Exercise NULL input through a real shell, and reload actual CSV exports."""

import csv
import io
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

CLI = str(Path(sys.argv.pop(1)).resolve())


class NullCliTest(unittest.TestCase):
    def test_shell_and_export_round_trip(self):
        with tempfile.TemporaryDirectory() as temporary:
            database = Path(temporary) / "database with spaces"

            def run(*arguments, success=True):
                arguments = [CLI, str(database), *arguments]
                if os.name == "nt":
                    # Windows PowerShell 5 uses legacy native argument passing:
                    # explicit double quotes are needed to pass an empty argument.
                    command = "& " + " ".join(
                        "'\"\"'" if arg == "" else "'" + arg.replace("'", "''") + "'"
                        for arg in arguments
                    )
                    invocation = ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command]
                else:
                    invocation = ["/bin/sh", "-c", shlex.join(arguments)]
                result = subprocess.run(invocation, capture_output=True, text=True)
                if success:
                    self.assertEqual(result.returncode, 0, result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0)
                return result.stdout

            run("create", "cells", "i:INTEGER:NOT_NULL", "t:TEXT", "v:VECTOR(2)")
            self.assertIn("nullable", run("describe", "cells"))
            payloads = [r"\N", "", "NULL", r"\\N", "quote' and comma,", "line1\nline2"]
            for number, text in enumerate(payloads, 1):
                run("insert", "cells", str(number), text, r"\N")
            run("insert", "cells", r"\N", "rejected", "[1,2]", success=False)
            run("insert", "cells", "7", r"\unescaped", "[1,2]", success=False)
            exported = run("select", "cells", "--csv")
            rows = list(csv.reader(io.StringIO(exported)))
            self.assertEqual(rows[0], ["row_id", "__vrdb_null_bitmap", "i", "t", "v"])
            self.assertEqual([row[1] for row in rows[1:]], ["011"] + ["001"] * 5)
            self.assertEqual([row[3] for row in rows[1:]], ["", "", "NULL", r"\N", "quote' and comma,", "line1\nline2"])
            displayed = run("select", "cells")
            self.assertIn('"NULL"', displayed)
            # The same loader used for persistence must accept an actual CLI export.
            (database / "tables" / "cells.csv").write_text(exported, encoding="utf-8", newline="")
            self.assertEqual(list(csv.reader(io.StringIO(run("select", "cells", "--csv")))), rows)
            run("update", "cells", "1", "1", "present", "[1,2]")
            run("update", "cells", "2", "2", r"\N", r"\N")
            run("update", "cells", "1", r"\N", "rejected", "[1,2]", success=False)
            rows = list(csv.reader(io.StringIO(run("select", "cells", "--csv"))))
            self.assertEqual(rows[1][1], "000")
            self.assertEqual(rows[2][1], "011")


if __name__ == "__main__":
    unittest.main()
