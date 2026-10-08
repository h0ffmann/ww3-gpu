import importlib.util, json, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("results", ROOT / "scripts" / "results.py")
results = importlib.util.module_from_spec(spec); spec.loader.exec_module(results)

RECORD = {"study": "s", "date": "2026-01-01", "routine": "R", "what": "w", "machine": "m",
          "toolchain": "t", "commit": "abc1234", "l1": "passed: L1_x", "statistic": "median of three",
          "runs": [{"backend": "Serial", "command": "cmd", "shim_ms": 2.5, "kernel_ms": 2.0}]}


class Results(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        (self.root / "bench" / "results").mkdir(parents=True)

    def write(self, record, name="s.json"):
        p = self.root / "bench" / "results" / name
        p.write_text(json.dumps(record), encoding="utf-8")
        return results.load(self.root / "bench" / "results")

    def test_a_valid_record_has_no_problems(self):
        self.assertEqual(results.problems(self.write(RECORD)), [])

    def test_a_timing_without_passed_l1_is_rejected(self):
        errs = results.problems(self.write(dict(RECORD, l1="not run")))
        self.assertTrue(any("without parity" in e for e in errs))

    def test_a_run_without_its_command_is_rejected(self):
        bad = dict(RECORD, runs=[{"backend": "Serial", "shim_ms": 1, "kernel_ms": 1}])
        self.assertIn("s.json: run 0 missing 'command'", results.problems(self.write(bad)))

    def test_marked_block_is_regenerated_with_a_relative_link(self):
        records = self.write(RECORD)
        page = self.root / "docs" / "p.md"
        page.parent.mkdir()
        page.write_text("x\n<!-- results:s:start -->\nold\n<!-- results:s:end -->\ny\n", encoding="utf-8")
        new, missing = results.render(page, records, self.root)
        self.assertEqual(missing, [])
        self.assertIn("| Serial | 2.5 | 2 | — | — |", new)
        self.assertIn("(../bench/results/s.json)", new)
        self.assertNotIn("old", new)

    def test_checked_in_tables_are_current(self):
        self.assertEqual(results.main(["check"]), 0)
