import importlib.util, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("adrs", ROOT / "scripts" / "adrs.py")
adrs = importlib.util.module_from_spec(spec); spec.loader.exec_module(adrs)


class AdrSummary(unittest.TestCase):
    def test_reads_title_status_and_date(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "ADR-0007-x.md").write_text(
            "# ADR-0007: Do the thing\n\n| | |\n|---|---|\n| **Status** | Accepted |\n"
            "| **Date** | 2026-01-02 |\n", encoding="utf-8")
        (d / "README.md").write_text("# not an ADR\n", encoding="utf-8")
        self.assertEqual(adrs.load(d), [("ADR-0007", "Accepted", "2026-01-02", "Do the thing")])

    def test_every_committed_adr_parses(self):
        for num, status, date, title in adrs.load():
            self.assertNotIn("?", (status, date), num)
            self.assertFalse(title.startswith("ADR-"), num)


if __name__ == "__main__":
    unittest.main()
