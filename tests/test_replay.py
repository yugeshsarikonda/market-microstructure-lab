"""Black-box checks of the compiled replay executable, without Python dependencies."""

import csv
import io
from pathlib import Path
import subprocess
import sys
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
ROOT = Path(__file__).resolve().parents[1]


def replay(text):
    return subprocess.run([BINARY, "-"], input=text, text=True, capture_output=True, timeout=10)


class ReplayTests(unittest.TestCase):
    def test_example_has_expected_trades_and_final_depth(self):
        result = replay((ROOT / "examples/orders.csv").read_text())
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, (ROOT / "examples/expected.csv").read_text())
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        trades = [(int(r["maker"]), int(r["taker"]), int(r["price_ticks"]), int(r["quantity"]))
                  for r in rows if r["type"] == "TRADE"]
        self.assertEqual(trades, [(1, 5, 101, 10), (2, 5, 101, 2), (3, 6, 103, 8),
                                  (7, 8, 100, 4), (4, 8, 99, 10), (4, 9, 99, 2)])
        depth = [r for r in rows if r["type"] == "DEPTH"]
        self.assertEqual(len(depth), 1)
        self.assertEqual((depth[0]["price_ticks"], depth[0]["quantity"], depth[0]["detail"]),
                         ("105", "6", "S orders=1"))

    def test_repeated_replay_is_byte_identical(self):
        text = (ROOT / "examples/orders.csv").read_text()
        self.assertEqual(replay(text).stdout, replay(text).stdout)

    def test_malformed_events_fail_with_line_numbers(self):
        invalid = ["LIMIT,1,B,100,0", "LIMIT,1,Q,100,1", "MARKET,1,B,100,2",
                   "LIMIT,-1,B,100,2", "LIMIT,1,B,1.5,2", "LIMIT,1,B,100,2,extra",
                   "LIMIT,1,B,100,9223372036854775808", "CANCEL,18446744073709551616",
                   "UNKNOWN,1", "LIMIT,1,B,100,", "LIMIT,1,B,+100,2"]
        for line in invalid:
            with self.subTest(line=line):
                result = replay("# comment\n" + line + "\n")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("line 2:", result.stderr)

    def test_duplicate_ids_fail_after_cancellation(self):
        result = replay("LIMIT,1,B,100,2\nCANCEL,1\nLIMIT,1,S,101,2\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("line 3:", result.stderr)
        self.assertIn("already used", result.stderr)

    def test_crlf_and_unknown_cancellation(self):
        result = replay("CANCEL,42\r\nMARKET,1,B,,2\r\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("not_found", result.stdout)
        self.assertIn("unfilled=2", result.stdout)

    def test_file_input_matches_stdin(self):
        file_result = subprocess.run([BINARY, str(ROOT / "examples/orders.csv")],
                                     text=True, capture_output=True, timeout=10)
        stdin_result = replay((ROOT / "examples/orders.csv").read_text())
        self.assertEqual(file_result.returncode, 0)
        self.assertEqual(file_result.stdout, stdin_result.stdout)

    def test_missing_file_and_wrong_usage_fail(self):
        result = subprocess.run([BINARY, str(ROOT / "does-not-exist.csv")], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        result = subprocess.run([BINARY], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
